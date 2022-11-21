/**
 * @file linear-regression-gd-ckks.cpp
 * @author your name (you@domain.com)
 * @brief Linear regression using gradient descent algorithm
 *        and CKKS Scheme.
 */

//==================================================================================
// BSD 2-Clause License
//
// Copyright (c) 2014-2022, NJIT, Duality Technologies Inc. and other contributors
//
// All rights reserved.
//
// Author TPOC: contact@openfhe.org
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//==================================================================================

#define PROFILE

#include "openfhe.h"

using namespace lbcrypto;

#define MAX_ENTRY 256
#define INC_FACTOR 1

const char* file_small = "/afs/andrew.cmu.edu/usr24/jiachend/public/cleaned_small_user_data.csv";

typedef struct {
    std::vector<int> id;
    std::vector<std::string> name;
    std::vector<double> age;
    std::vector<double> income;
    size_t size;
} data_t;

/**
 * @brief Get the Next Line And Split Into Tokens
 * 
 * https://stackoverflow.com/questions/1120140/how-can-i-read-and-parse-csv-files-in-c
 * 
 * @param str File Stream
 * @return std::vector<std::string>
 */
std::vector<std::string> getNextLine(std::istream& str)
{
    std::vector<std::string>   result;
    std::string                line;
    std::getline(str,line);

    std::stringstream          lineStream(line);
    std::string                cell;

    while(std::getline(lineStream,cell, ','))
    {
        result.push_back(cell);
    }
    // This checks for a trailing comma with no data after it.
    if (!lineStream && cell.empty())
    {
        // If there was a trailing comma then add an empty element.
        result.push_back("");
    }
    return result;
}

void init_data(data_t *data) {
    data->size = 0;
    data->id.clear();
    data->name.clear();
    data->age.clear();
    data->income.clear();
}

/**
 * @brief Read the age-income data file
 * 
 * @param filename The name of the file
 * @param data Structure of data to be filled in.
 */
void readFile(const char* filename, data_t *data) {
    std::ifstream myfile;
    myfile.open(filename);
    auto line = getNextLine(myfile);
    size_t fields = line.size();
    line = getNextLine(myfile);
    // data->size = 0;
    // data->id.clear();
    // data->name.clear();
    // data->age.clear();
    // data->income.clear();
    while(line.size() == fields && data->size < MAX_ENTRY) {
        data->id.push_back(std::stoi(line[0]));
        data->name.push_back(line[1]);
        data->age.push_back(std::stod(line[2]));
        data->income.push_back(std::stod(line[3]) / INC_FACTOR);
        data->size++;
        line = getNextLine(myfile);
    }
    myfile.close();
}

int main() {
    data_t *data = new data_t;
    init_data(data);
    for(int i = 0; i < 100; i++)
        readFile(file_small, data);
    
    std::cout << "DATA SIZE: " << data->size << std::endl;

    usint batch_size = data->size;

    // Sample Program: Step 1 - Set CryptoContext
    CCParams<CryptoContextCKKSRNS> parameters;
    /**
     * @brief We want to use a very large prime so that every intermediate
     * result will not be moduled? As trying a small module produce incorrect
     * output, this prime is found in:
     * https://oeis.org/A182300
     * 
     * Weird enough.. though.
     * 
     * @todo Find a weirdly big prime number satisfy (p-1)/32768 is integer?
     * @todo Dig into more about what is the use of a integer here?
     */
    // TODO: 
    // Dig into more about what this is about?
    // parameters.SetPlaintextModulus(536903681);
    SecretKeyDist secretKeyDist = UNIFORM_TERNARY;
    parameters.SetSecretKeyDist(secretKeyDist);

    parameters.SetBatchSize(batch_size);
    // We have at most 3 multiplication depth for linear regression.. I guess?
    parameters.SetMultiplicativeDepth(50);
    parameters.SetScalingModSize(52);
    parameters.SetScalingTechnique(FLEXIBLEAUTO);
    parameters.SetFirstModSize(60);
    parameters.SetRingDim(65536);
    
    std::vector<uint32_t> bsgsDim = {0, 0};
    std::vector<uint32_t> levelBudget = {4, 4};

    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
    
    std::cout << "CKKS scheme ring dimension " << cc->GetRingDimension() << std::endl;
    std::cout << FHECKKSRNS::GetBootstrapDepth(9, levelBudget, secretKeyDist) << std::endl;
    // Enable features
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);
    cc->Enable(FHE);

    cc->EvalBootstrapSetup(levelBudget, bsgsDim, batch_size);


    // Sample Program: Step 2 - Key Generation

    // Initialize Public Key Containers
    KeyPair<DCRTPoly> keyPair;

    // Generate a public/private key pair
    keyPair = cc->KeyGen();

    // Generate the relinearization key
    cc->EvalMultKeyGen(keyPair.secretKey);

    // Esential for summing up the array.
    cc->EvalSumKeyGen(keyPair.secretKey);
    cc->EvalBootstrapKeyGen(keyPair.secretKey, batch_size);

    TimeVar t;
    TIC(t);
    // Sample Program: Step 3 - Encryption
    std::cout << "Encrypting plaintexts..." << std::endl;
    // Plaint text for age and income
    Plaintext pt_age    = cc->MakeCKKSPackedPlaintext(data->age);
    Plaintext pt_income = cc->MakeCKKSPackedPlaintext(data->income);
    /**
     * @brief A tricky way to place a plain text integer inside
     * calculation, as it throws NotImplmentedError if we directly
     * use EvalMult(ct, batch_size)..
     * 
     * @todo Try a better way to do multiplication between ciphertext
     * and plain text.
     */

    std::vector<double> m_vec(1);
    Plaintext pt_m    = cc->MakeCKKSPackedPlaintext(m_vec);

    std::vector<double> c_vec(1);
    Plaintext pt_c    = cc->MakeCKKSPackedPlaintext(c_vec);

    double L = 0.0001;
    
    // The encoded vectors are encrypted
    auto ct_age      = cc->Encrypt(keyPair.publicKey, pt_age);
    auto ct_income   = cc->Encrypt(keyPair.publicKey, pt_income);
    auto ct_m        = cc->Encrypt(keyPair.publicKey, pt_m);
    auto ct_c        = cc->Encrypt(keyPair.publicKey, pt_c);
    

    TOC(t);
    std::cout << "Plaintext encrypted! Time used: " << TOC(t) << "ms" << std::endl;

    // Sample Program: Step 4 - Evaluation
    TIC(t);
    // Calculate coefficients
    std::cout << "Calculating coefficients..." << std::endl;
    

    int epoches = 15;
    for(int iter = 0; iter < epoches; iter++) {

        auto ct_pred = cc->EvalMult(ct_age, ct_m);
        ct_pred = cc->EvalAdd(ct_pred, ct_c);

        auto ct_dif = cc->EvalSub(ct_income, ct_pred);
        auto ct_dm = cc->EvalMult(ct_age, ct_dif);
        auto ct_dc = cc->EvalSum(ct_dif, batch_size);
        
        ct_dm = cc->EvalSum(ct_dm, batch_size);

        ct_dm = cc->EvalMult(ct_dm, (-2.0 * L / batch_size));
        ct_dc = cc->EvalMult(ct_dc, (-2.0 * L / batch_size));

        ct_dc->SetSlots(1);
        ct_dm->SetSlots(1);

        ct_m = cc->EvalSub(ct_m, ct_dm);
        ct_c = cc->EvalSub(ct_c, ct_dc);


        cc->Decrypt(keyPair.secretKey, ct_m, &pt_m);
        cc->Decrypt(keyPair.secretKey, ct_c, &pt_c);
        pt_m->SetLength(1);
        pt_c->SetLength(1);
        std::cout << "m: " << pt_m;
        std::cout << "c: " << pt_c;
        std::cout << "Depth m: " << ct_m->GetLevel();
        std::cout << " Depth c: " << ct_c->GetLevel();
        std::cout << std::endl;
        if(ct_m->GetLevel() > 30) {
            ct_m = cc->EvalBootstrap(ct_m);
            ct_c = cc->EvalBootstrap(ct_c);
        }
    }


    std::cout << "Coefficients calcuated! Time used: " << TOC(t) << "ms" << std::endl;
    return 0;
}
