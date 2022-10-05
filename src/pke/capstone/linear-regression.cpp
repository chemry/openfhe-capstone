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

/*
  Simple examples for CKKS
 */

#define PROFILE

#include "openfhe.h"

using namespace lbcrypto;

#define MAX_ENTRY 2000
#define INC_FACTOR 10

const char* file_small = "/afs/andrew.cmu.edu/usr24/jiachend/public/cleaned_small_user_data.csv";

typedef struct {
    std::vector<int> id;
    std::vector<std::string> name;
    std::vector<int64_t> age;
    std::vector<int64_t> income;
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
    data->size = 0;
    data->id.clear();
    data->name.clear();
    data->age.clear();
    data->income.clear();
    while(line.size() == fields && data->size < MAX_ENTRY) {
        data->id.push_back(std::stoi(line[0]));
        data->name.push_back(line[1]);
        data->age.push_back(std::stoi(line[2]));
        data->income.push_back(std::stoi(line[3]) / INC_FACTOR);
        data->size++;
        line = getNextLine(myfile);
    }
    myfile.close();
}

int main() {
    data_t *data = new data_t;
    readFile(file_small, data);

    std::cout << "DATA SIZE: " << data->size << std::endl;

    usint batch_size = MAX_ENTRY;

    // Sample Program: Step 1 - Set CryptoContext
    CCParams<CryptoContextBFVRNS> parameters;
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
    parameters.SetPlaintextModulus(140737471578113);
    parameters.SetBatchSize(batch_size);
    // We have at most 3 multiplication depth for linear regression.. I guess?
    parameters.SetMultiplicativeDepth(3);
    parameters.SetScalingModSize(50);
    parameters.SetMaxRelinSkDeg(3);
    

    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
    // Enable features
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);

    // Sample Program: Step 2 - Key Generation

    // Initialize Public Key Containers
    KeyPair<DCRTPoly> keyPair;

    // Generate a public/private key pair
    keyPair = cc->KeyGen();

    // Generate the relinearization key
    cc->EvalMultKeyGen(keyPair.secretKey);

    // Esential for summing up the array.
    cc->EvalSumKeyGen(keyPair.secretKey);

    TimeVar t;
    TIC(t);
    // Sample Program: Step 3 - Encryption
    std::cout << "Encrypting plaintexts..." << std::endl;
    // Plaint text for age and income
    Plaintext pt_age    = cc->MakePackedPlaintext(data->age);
    Plaintext pt_income = cc->MakePackedPlaintext(data->income);
    /**
     * @brief A tricky way to place a plain text integer inside
     * calculation, as it throws NotImplmentedError if we directly
     * use EvalMult(ct, batch_size)..
     * 
     * @todo Try a better way to do multiplication between ciphertext
     * and plain text.
     */
    std::vector<int64_t> size_int = {MAX_ENTRY};
    Plaintext pt_size   = cc->MakePackedPlaintext(size_int);
    
    // The encoded vectors are encrypted
    auto ct_age    = cc->Encrypt(keyPair.publicKey, pt_age);
    auto ct_income = cc->Encrypt(keyPair.publicKey, pt_income);
    auto ct_size   = cc->Encrypt(keyPair.publicKey, pt_size);
    
    TOC(t);
    std::cout << "Plaintext encrypted! Time used: " << TOC(t) << "ms" << std::endl;

    // Sample Program: Step 4 - Evaluation
    TIC(t);
    // Calculate coefficients
    std::cout << "Calculating coefficients..." << std::endl;
    auto ct_sigx   = cc->EvalSum(ct_age, batch_size);
    /** 
     * @brief I'm not sure whether is useful for setslots here.. 
     * it seems not quite useful. Since Eval Sum produces
     * an array like: [sum(0,n), sum(1,n), sum(2,n)... ], where
     * the rest of array seems useless.
     * 
     * @todo Found a better way to just keep the first element.
     */
    ct_sigx->SetSlots(1);
    auto ct_sigy   = cc->EvalSum(ct_income, batch_size);
    ct_sigy->SetSlots(1);

    auto ct_x2     = cc->EvalMult(ct_age, ct_age);
    auto ct_y2     = cc->EvalMult(ct_income, ct_income);
    auto ct_xy     = cc->EvalMult(ct_income, ct_age);

    auto ct_sig_x2 = cc->EvalSum(ct_x2, batch_size);
    ct_sig_x2->SetSlots(1);
    auto ct_sig_xy = cc->EvalSum(ct_xy, batch_size);
    ct_sig_xy->SetSlots(1);

    auto ct_a0     = cc->EvalMult(ct_sigy, ct_sig_x2);
    auto ct_a1     = cc->EvalMult(ct_sigx, ct_sig_xy);
    auto ct_a      = cc->EvalSub(ct_a0, ct_a1);

    auto ct_b1     = cc->EvalMult(ct_sig_xy, ct_size);
    auto ct_b2     = cc->EvalMult(ct_sigx, ct_sigy);
    auto ct_b      = cc->EvalSub(ct_b1, ct_b2);

    auto ct_div1   = cc->EvalMult(ct_size, ct_sig_x2);
    auto ct_div2   = cc->EvalMult(ct_sigx, ct_sigx);
    auto ct_div    = cc->EvalSub(ct_div1, ct_div2);

    std::cout << "Coefficients calcuated! Time used: " << TOC(t) << "ms" << std::endl;

    // Decrypt the results
    Plaintext pt_a;
    Plaintext pt_b;
    Plaintext pt_div;
    cc->Decrypt(keyPair.secretKey, ct_a, &pt_a);
    cc->Decrypt(keyPair.secretKey, ct_b, &pt_b);
    cc->Decrypt(keyPair.secretKey, ct_div, &pt_div);

    // We only need one element for output
    pt_a->SetLength(1);
    pt_b->SetLength(1);
    pt_div->SetLength(1);


    // ==== This if for customer side ======
    std::cout << "\nResults of homomorphic computations" << std::endl;
    std::cout << "a: " << pt_a << std::endl;
    std::cout << "b: " << pt_b << std::endl;
    std::cout << "div: " << pt_div << std::endl;

    // Get our integer back
    int64_t a = pt_a->GetPackedValue()[0];
    int64_t b = pt_b->GetPackedValue()[0];
    int64_t div = pt_div->GetPackedValue()[0];

    // As BFV does not support floating point calculations,
    // we leave it to customer to do this.
    double intercept = (double)a / div * INC_FACTOR;
    double coef      = (double)b / div * INC_FACTOR;
    std::cout << "inter, coef: " << intercept << ", " << coef << std::endl;

    return 0;
}
