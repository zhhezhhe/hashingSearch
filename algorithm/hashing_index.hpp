#ifndef EFANNA_HASHING_INDEX_H_
#define EFANNA_HASHING_INDEX_H_

#include "algorithm/base_index.hpp"
#include <boost/dynamic_bitset.hpp>
#include <time.h>
//for Debug
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <map>
#include <sstream>
#include <set>
#include <bitset>
//#define MAX_RADIUS 6


namespace efanna{
struct HASHINGIndexParams : public IndexParams
{
	HASHINGIndexParams(int codelen, int tablelen, char*& BaseCodeFile, char*& QueryCodeFile, int index_method=1)
	{
		init_index_type = HASHING;
		ValueType len;
		len.int_val = codelen;
		extra_params.insert(std::make_pair("codelen",len));
		ValueType tablen;
		tablen.int_val = tablelen;
		extra_params.insert(std::make_pair("tablelen",tablen));
		ValueType bcf;
		bcf.str_pt = BaseCodeFile;
		extra_params.insert(std::make_pair("bcfile",bcf));
		ValueType qcf;
		qcf.str_pt = QueryCodeFile;
		extra_params.insert(std::make_pair("qcfile",qcf));
		ValueType method;
		method.int_val = index_method;
		extra_params.insert(std::make_pair("method",method));
	}
};

template <typename DataType>
class HASHINGIndex : public InitIndex<DataType>
{
public:

	typedef InitIndex<DataType> BaseClass;

	typedef std::vector<unsigned int> Code;
	typedef std::vector<Code> Codes;
	typedef std::unordered_map<unsigned int, std::vector<unsigned int> > HashBucket;
	typedef std::vector<HashBucket> HashTable;

	typedef std::vector<unsigned long> Code64;
	typedef std::vector<Code64> Codes64;
	typedef std::unordered_map<unsigned long, std::vector<unsigned int> > HashBucket64;
	typedef std::vector<HashBucket64> HashTable64;


	HASHINGIndex(const Matrix<DataType>& dataset, const Distance<DataType>* d, const IndexParams& params = HASHINGIndexParams(0,NULL,NULL)) :
		BaseClass(dataset,d,params)
	{

		ExtraParamsMap::const_iterator it = params_.extra_params.find("codelen");
		if(it != params_.extra_params.end()){
			codelength = (it->second).int_val;
			std::cout << "use  "<<codelength<< " bit code"<< std::endl;
		}
		else{
			std::cout << "error: no code length setting" << std::endl;
		}

		it = params_.extra_params.find("tablelen");
		if(it != params_.extra_params.end()){
			tablelen = (it->second).int_val;
			if(tablelen>64){
				std::cout<<"max table length: 54; " <<std::endl;
				tablelen = 64;
			}
		}
		else{
			std::cout << "error: no table lenth setting, will use 32 bits table" << std::endl;
			tablelen = 32;
		}
		std::cout << "use  "<<tablelen<< " bits tables"<< std::endl;

		if(tablelen<=36){
			radius = 11;
		}else if(tablelen<=44){
			radius = 9;
		}else if(tablelen<=52){
			radius = 8;
		}else{
			radius = 7;
		}
		std::cout << "search hamming radius "<<radius<< std::endl;

		it = params_.extra_params.find("bcfile");
		if(it != params_.extra_params.end()){
			char* fpath = (it->second).str_pt;
			std::string str(fpath);
			std::cout << "Loading base code from " << str << std::endl;

			LoadCode(fpath, BaseCodeOrig);
		}
		else{
			std::cout << "error: no base code file" << std::endl;
		}

		it = params_.extra_params.find("qcfile");
		if(it != params_.extra_params.end()){
			char* fpath = (it->second).str_pt;
			std::string str(fpath);
			std::cout << "Loading query code from " << str << std::endl;

			LoadCode(fpath, QueryCodeOrig);

		}
		else{
			std::cout << "error: no query code file" << std::endl;
		}

		it = params_.extra_params.find("method");
		if(it != params_.extra_params.end()){
			index_method = (it->second).int_val;
		}
		if(index_method > 1){
			index_method = 1;
		}


		if(tablelen > 16){
			upbits = 16;
		}else{
			upbits = tablelen - 2;
		}

	}



	/**
	 * Builds the index
	 */

	void LoadCode(char* filename, Codes& base){
		unsigned tableNum = codelength / 32;
		unsigned lastLen = codelength % 32;

		std::stringstream ss;
		unsigned j;
		for(j = 0; j < tableNum; j++){

			ss << filename << "_" << j+1 ;
			std::string codeFile;
			ss >> codeFile;
			ss.clear();

			std::ifstream in(codeFile.c_str(), std::ios::binary);
			if(!in.is_open()){std::cout<<"open file " << filename <<" error"<< std::endl;return;}

			int codeNum;
			in.read((char*)&codeNum,4);
			if (codeNum != 1){
				std::cout<<"codefile  "<< j << " error!"<<std::endl;
			}

			unsigned len;
			in.read((char*)&len,4);
			unsigned num;
			in.read((char*)&num,4);

			Code b;
			for(unsigned i = 0; i < num; i++){
				unsigned int codetmp;
				in.read((char*)&codetmp,4);
				b.push_back(codetmp);
			}
			base.push_back(b);
			in.close();
		}

		if(lastLen > 0){
			int shift = 32 - lastLen;
			ss << filename << "_" << j+1 ;
			std::string codeFile;
			ss >> codeFile;
			ss.clear();

			std::ifstream in(codeFile.c_str(), std::ios::binary);
			if(!in.is_open()){std::cout<<"open file " << filename <<" error"<< std::endl;return;}

			int codeNum;
			in.read((char*)&codeNum,4);
			if (codeNum != 1){
				std::cout<<"codefile  "<< j << " error!"<<std::endl;
			}

			unsigned len;
			in.read((char*)&len,4);
			unsigned num;
			in.read((char*)&num,4);

			Code b;
			for(unsigned i = 0; i < num; i++){
				unsigned int codetmp;
				in.read((char*)&codetmp,4);
				codetmp = codetmp >> shift;
				b.push_back(codetmp);
			}
			base.push_back(b);
			in.close();
		}



	}

	void ConvertCode(Codes& baseOrig, Codes& base, int tablelen){

		unsigned pNum = baseOrig[0].size();
		unsigned nTableOrig = baseOrig.size();

		for (unsigned i=0; i<nTableOrig; i++){
			Code table(pNum);
			std::fill(table.begin(), table.end(), 0);
			base.push_back(table);
		}

		int shift = 32 - tablelen;
		for (unsigned i = 0; i < pNum; i++) {
			for (unsigned j = 0; j < nTableOrig; j++) {
				base[j][i] = baseOrig[j][i] >> shift;
			}
		}
	}

	/*void ConvertCode3(Codes& baseOrig, Codes& base, int tablelen){

		unsigned pNum = baseOrig[0].size();
		unsigned nTableOrig = baseOrig.size();

		for (unsigned i=0; i<nTableOrig; i++){
			Code table(pNum);
			std::fill(table.begin(), table.end(), 0);
			base.push_back(table);
		}

		if(tablelen == 32){
			for (unsigned i = 0; i < pNum; i++) {
				for (unsigned j = 0; j < nTableOrig; j++) {
					base[j][i] = baseOrig[j][i];
				}
			}
		}else{
			for (unsigned i = 0; i < pNum; i++) {
				for (unsigned j = 0; j < nTableOrig; j++) {
					base[j][i] = baseOrig[j][i] & ((1 << tablelen) - 1);
				}
			}
		}
	}*/

	void ConvertCode1(Codes& baseOrig, Codes& base, int tablelen){

		unsigned pNum = baseOrig[0].size();
		unsigned nTableOrig = baseOrig.size();

		/*unsigned tableNum = codelength / tablelen;
		unsigned lastLen = codelength % tablelen;

		if(lastLen > 0){
			for (unsigned i=0; i<tableNum+1; i++){
				Code table(pNum);
				std::fill(table.begin(), table.end(), 0);
				base.push_back(table);
			}
		}else{
			for (unsigned i=0; i<tableNum; i++){
				Code table(pNum);
				std::fill(table.begin(), table.end(), 0);
				base.push_back(table);
			}
		}*/
		unsigned tableNum = codelength / tablelen;

		for (unsigned i=0; i<tableNum; i++){
			Code table(pNum);
			std::fill(table.begin(), table.end(), 0);
			base.push_back(table);
		}


		for (unsigned i = 0; i < pNum; i++) {
			unsigned int tableidx = 0;
			unsigned int codePre = 0;
			unsigned int lenPre = 0;
			unsigned int codeRemain = 0;
			unsigned int remain = 0;
			for (unsigned j = 0; j < nTableOrig-1; j++) {
				remain = 32;
				codeRemain = baseOrig[j][i];
				//std::cout <<"Orig"<<i<<" "<< std::bitset<32>(codeRemain) << std::endl;
				if(lenPre > 0){
					unsigned int need = tablelen-lenPre;
					base[tableidx][i] = codePre << need;
					remain = remain - need;
					base[tableidx][i] += codeRemain >> remain;
					tableidx ++;
					codeRemain = codeRemain & ((1 << remain) - 1);
				}
				while(remain >= (unsigned) tablelen){
					remain = remain - tablelen;
					base[tableidx][i] = codeRemain >> remain;
					//std::cout << std::bitset<32>(base[tableidx][i]) << std::endl;
					tableidx ++;
					codeRemain = codeRemain & ((1 << remain) - 1);
				}
				lenPre = remain;
				codePre = codeRemain;
			}

			codeRemain = baseOrig[nTableOrig-1][i];
			remain = 32;
			if(lenPre > 0){
				unsigned int need = tablelen-lenPre;
				base[tableidx][i] = codePre << need;
				remain = remain - need;
				base[tableidx][i] += codeRemain >> remain;
				tableidx ++;
				codeRemain = codeRemain & ((1 << remain) - 1);
			}
			while(remain >= (unsigned) tablelen){
				remain = remain - tablelen;
				base[tableidx][i] = codeRemain >> remain;
				tableidx ++;
				codeRemain = codeRemain & ((1 << remain) - 1);
			}

			/*remain = codelength - 32*(nTableOrig - 1);
			if(remain < (unsigned) tablelen){
				std::cout << "Not implemented! " << std::endl;
			}
			if (lenPre + remain > (unsigned) tablelen){
				if(lenPre > 0){
					unsigned int need = tablelen-lenPre;
					base[tableidx][i] = codePre << need;
					remain = remain - need;
					base[tableidx][i] += codeRemain >> remain;
					tableidx ++;
					codeRemain = codeRemain & ((1 << remain) - 1);
				}
				while(remain >= (unsigned) tablelen){
					remain = remain - tablelen;
					base[tableidx][i] = codeRemain >> remain;
					//std::cout << std::bitset<32>(base[tableidx][i]) << std::endl;
					tableidx ++;
					codeRemain = codeRemain & ((1 << remain) - 1);
				}
				if(remain > 0){
					codeRemain = baseOrig[nTableOrig-1][i];
					base[tableidx][i] = codeRemain & ((1<< tablelen)-1);
				}
			}else{
				if(lenPre > 0){
					unsigned int need = tablelen-lenPre;
					base[tableidx][i] = codePre << need;
				}
				base[tableidx][i] += codeRemain;
			}*/

		}
	}

	/*void ConvertCode2(Codes& baseOrig, Codes& base, int tablelen){

		unsigned pNum = baseOrig[0].size();
		unsigned nTableOrig = baseOrig.size();

		unsigned tableNum = codelength / tablelen;
		unsigned lastLen = codelength % tablelen;

		if(lastLen > 0){
			for (unsigned i=0; i<tableNum+1; i++){
				Code table(pNum);
				std::fill(table.begin(), table.end(), 0);
				base.push_back(table);
			}
		}else{
			for (unsigned i=0; i<tableNum; i++){
				Code table(pNum);
				std::fill(table.begin(), table.end(), 0);
				base.push_back(table);
			}
		}

		int shift = 32 - tablelen;
		for (unsigned i = 0; i < pNum; i++) {
			for (unsigned j = 0; j < nTableOrig; j++) {
				base[j][i] = baseOrig[j][i] >> shift;
			}
		}


		if (shift > 0){
			for (unsigned i = 0; i < pNum; i++) {
				unsigned tableidx = nTableOrig;
				unsigned codePre = 0;
				unsigned lenPre = 0;
				unsigned codeRemain = 0;
				unsigned remain = 0;
				unsigned j = 0;
				unsigned need = tablelen;
				while (j < nTableOrig-1) {
					if(lenPre > 0){
						need = need-lenPre;
						base[tableidx][i] = codePre << need;
						lenPre = 0;
					}
					remain = shift;
					codeRemain = baseOrig[j][i];
					codeRemain = codeRemain & ((1 << shift) - 1);
					j++;
					while(need > 0 && remain <= need && j < nTableOrig-1){
						need = need - remain;
						base[tableidx][i] += codeRemain << need;
						remain = shift;
						codeRemain = baseOrig[j][i];
						codeRemain = codeRemain & ((1 << shift) - 1);
						j++;
					}
					if(remain > need){
						lenPre = remain - need;
						if(need > 0){
							codePre = codeRemain & ((1 << lenPre) - 1);
							base[tableidx][i] += codeRemain >> lenPre;
						}else{
							codePre = codeRemain;
						}
						tableidx++;
						need = tablelen;
					}
				}

				if(lenPre > 0){
					need = tablelen-lenPre;
					base[tableidx][i] = codePre << need;
					lenPre = 0;
				}
				remain = shift;
				codeRemain = baseOrig[nTableOrig-1][i];
				codeRemain = codeRemain & ((1 << shift) - 1);
				if(remain >= need){
					remain = remain - need;
					base[tableidx][i] += codeRemain >> remain;
					tableidx++;
					need = tablelen;
					codeRemain = codeRemain & ((1 << remain) - 1);
				}

				if(remain > 0){
					codeRemain = baseOrig[nTableOrig-1][i];
					base[tableidx][i] = codeRemain & ((1 << tablelen) - 1);
				}
			}
		}

	}*/

	void ConvertCode64(Codes& baseOrig, Codes64& base, int tablelen){

		unsigned pNum = baseOrig[0].size();
		unsigned nTableOrig = baseOrig.size();

		unsigned tableNum = codelength / tablelen;
		for (unsigned i=0; i<tableNum; i++){
			Code64 table(pNum);
			std::fill(table.begin(), table.end(), 0);
			base.push_back(table);
		}

		/*unsigned tableNum = codelength / tablelen;
		unsigned lastLen = codelength % tablelen;

		if(lastLen > 0){
			for (unsigned i=0; i<tableNum+1; i++){
				Code64 table(pNum);
				std::fill(table.begin(), table.end(), 0);
				base.push_back(table);
			}
		}else{
			for (unsigned i=0; i<tableNum; i++){
				Code64 table(pNum);
				std::fill(table.begin(), table.end(), 0);
				base.push_back(table);
			}
		}*/

		for (unsigned i = 0; i < pNum; i++) {
		//for (unsigned i = 0; i < 1; i++) {
			unsigned tableidx = 0;
			unsigned long codePre = 0;
			unsigned lenPre = 0;
			unsigned long codeRemain = 0;
			unsigned remain = 0;
			unsigned j = 0;
			unsigned need = tablelen;
			while (j < nTableOrig && tableidx<tableNum) {
				if(lenPre > 0){
					//std::cout << "pre: "<< lenPre << std::endl;
					need = need-lenPre;
					//if(tableidx==tableNum) std::cout<<"tableidx out 1! "<< "j: "<< j<<std::endl;
					base[tableidx][i] = codePre << need;
					lenPre = 0;
				}
				remain = 32;
				//std::cout << "j: "<< j << std::endl;
				codeRemain = baseOrig[j][i];
				j++;
				while(need > 0 && remain <= need && j < nTableOrig){
					need = need - remain;
					//if(tableidx==tableNum) std::cout<<"tableidx out 2! "<< "j: "<< j<<std::endl;
					base[tableidx][i] += codeRemain << need;
					remain = 32;
					//std::cout << "j: "<< j << std::endl;
					codeRemain = baseOrig[j][i];
					j++;
				}
				if(remain > 0){
					if(remain > need){
						lenPre = remain - need;
						if(need > 0){
							codePre = codeRemain & ((1 << lenPre) - 1);
							//if(tableidx==tableNum) std::cout<<"tableidx out 3! "<< "j: "<< j<<std::endl;
							base[tableidx][i] += codeRemain >> lenPre;
						}else{
							codePre = codeRemain;
						}
						tableidx++;
						need = tablelen;
					}else{
						need = need - remain;
						//if(tableidx==tableNum) std::cout<<"tableidx out 4! "<< "j: "<< j<<std::endl;
						base[tableidx][i] += codeRemain << need;
					}
				}
			}

		}
	}


	void BuildHashTable32(int upbits, int lowbits, Codes& baseAll ,std::vector<HashTable>& tbAll){

		for(size_t h=0; h < baseAll.size(); h++){
			Code& base = baseAll[h];

			HashTable tb;
			for(int i = 0; i < (1 << upbits); i++){
				HashBucket emptyBucket;
				tb.push_back(emptyBucket);
			}

			for(size_t i = 0; i < base.size(); i ++){
				unsigned int idx1 = base[i] >> lowbits;
				unsigned int idx2 = base[i] & ((1 << lowbits) - 1);
				if(tb[idx1].find(idx2) != tb[idx1].end()){
					tb[idx1][idx2].push_back(i);
				}else{
					std::vector<unsigned int> v;
					v.push_back(i);
					tb[idx1].insert(make_pair(idx2,v));
				}
			}
			tbAll.push_back(tb);
		}
	}

	void BuildHashTable64(int upbits, int lowbits, Codes64& baseAll ,std::vector<HashTable64>& tbAll){

		unsigned long One = 1;

		for(size_t h=0; h < baseAll.size(); h++){
			Code64& base = baseAll[h];

			HashTable64 tb;
			for(int i = 0; i < (1 << upbits); i++){
				HashBucket64 emptyBucket;
				tb.push_back(emptyBucket);
			}

			for(size_t i = 0; i < base.size(); i++){
				unsigned int idx1 = base[i] >> lowbits;
				unsigned long idx2 = base[i] & ((One << lowbits) - 1);
				if(tb[idx1].find(idx2) != tb[idx1].end()){
					tb[idx1][idx2].push_back(i);
				}else{
					std::vector<unsigned int> v;
					v.push_back(i);
					tb[idx1].insert(make_pair(idx2,v));
				}
			}
			tbAll.push_back(tb);
		}
	}

	void generateMask32(){
		//i = 0 means the origin code
		HammingBallMask.push_back(0);
		HammingRadius.push_back(HammingBallMask.size());

		if(radius>0){
			//radius 1
			for(int i = 0; i < tablelen; i++){
				unsigned int mask = 1 << i;
				HammingBallMask.push_back(mask);
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>1){
			//radius 2
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					unsigned int mask = (1<<i) | (1<<j);
					HammingBallMask.push_back(mask);
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>2){
			//radius 3
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					for(int k = j+1; k < tablelen; k++){
						unsigned int mask = (1<<i) | (1<<j) | (1<<k);
						HammingBallMask.push_back(mask);
					}
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>3){
			//radius 4
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					for(int k = j+1; k < tablelen; k++){
						for(int a = k+1; a < tablelen; a++){
							unsigned int mask = (1<<i) | (1<<j) | (1<<k)| (1<<a);
							HammingBallMask.push_back(mask);
						}
					}
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>4){
			//radius 5
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					for(int k = j+1; k < tablelen; k++){
						for(int a = k+1; a < tablelen; a++){
							for(int b = a+1; b < tablelen; b++){
								unsigned int mask = (1<<i) | (1<<j) | (1<<k)| (1<<a)| (1<<b);
								HammingBallMask.push_back(mask);
							}
						}
					}
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>5){
			//radius 6
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					for(int k = j+1; k < tablelen; k++){
						for(int a = k+1; a < tablelen; a++){
							for(int b = a+1; b < tablelen; b++){
								for(int c = b+1; c < tablelen; c++){
									unsigned int mask = (1<<i) | (1<<j) | (1<<k)| (1<<a)| (1<<b)| (1<<c);
									HammingBallMask.push_back(mask);
								}
							}
						}
					}
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>6){
			//radius 7
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					for(int k = j+1; k < tablelen; k++){
						for(int a = k+1; a < tablelen; a++){
							for(int b = a+1; b < tablelen; b++){
								for(int c = b+1; c < tablelen; c++){
									for(int d = c+1; d < tablelen; d++){
										unsigned int mask = (1<<i) | (1<<j) | (1<<k)| (1<<a)| (1<<b)| (1<<c)| (1<<d);
										HammingBallMask.push_back(mask);
									}
								}
							}
						}
					}
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>7){
			//radius 8
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					for(int k = j+1; k < tablelen; k++){
						for(int a = k+1; a < tablelen; a++){
							for(int b = a+1; b < tablelen; b++){
								for(int c = b+1; c < tablelen; c++){
									for(int d = c+1; d < tablelen; d++){
										for(int e = d+1; e < tablelen; e++){
											unsigned int mask = (1<<i) | (1<<j) | (1<<k)| (1<<a)| (1<<b)| (1<<c)| (1<<d)| (1<<e);
											HammingBallMask.push_back(mask);
										}
									}
								}
							}
						}
					}
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>8){
			//radius 9
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					for(int k = j+1; k < tablelen; k++){
						for(int a = k+1; a < tablelen; a++){
							for(int b = a+1; b < tablelen; b++){
								for(int c = b+1; c < tablelen; c++){
									for(int d = c+1; d < tablelen; d++){
										for(int e = d+1; e < tablelen; e++){
											for(int f = e+1; f < tablelen; f++){
												unsigned int mask = (1<<i) | (1<<j) | (1<<k)| (1<<a)| (1<<b)| (1<<c)| (1<<d)| (1<<e)| (1<<f);
												HammingBallMask.push_back(mask);
											}
										}
									}
								}
							}
						}
					}
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>9){
			//radius 10
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					for(int k = j+1; k < tablelen; k++){
						for(int a = k+1; a < tablelen; a++){
							for(int b = a+1; b < tablelen; b++){
								for(int c = b+1; c < tablelen; c++){
									for(int d = c+1; d < tablelen; d++){
										for(int e = d+1; e < tablelen; e++){
											for(int f = e+1; f < tablelen; f++){
												for(int g = f+1; g < tablelen; g++){
													unsigned int mask = (1<<i) | (1<<j) | (1<<k)| (1<<a)| (1<<b)| (1<<c)| (1<<d)| (1<<e)| (1<<f)| (1<<g);
													HammingBallMask.push_back(mask);
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>10){
			//radius 11
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					for(int k = j+1; k < tablelen; k++){
						for(int a = k+1; a < tablelen; a++){
							for(int b = a+1; b < tablelen; b++){
								for(int c = b+1; c < tablelen; c++){
									for(int d = c+1; d < tablelen; d++){
										for(int e = d+1; e < tablelen; e++){
											for(int f = e+1; f < tablelen; f++){
												for(int g = f+1; g < tablelen; g++){
													for(int h = g+1; h < tablelen; h++){
														unsigned int mask = (1<<i) | (1<<j) | (1<<k)| (1<<a)| (1<<b)| (1<<c)| (1<<d)| (1<<e)| (1<<f)| (1<<g)| (1<<h);
														HammingBallMask.push_back(mask);
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>11){
			//radius 12
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					for(int k = j+1; k < tablelen; k++){
						for(int a = k+1; a < tablelen; a++){
							for(int b = a+1; b < tablelen; b++){
								for(int c = b+1; c < tablelen; c++){
									for(int d = c+1; d < tablelen; d++){
										for(int e = d+1; e < tablelen; e++){
											for(int f = e+1; f < tablelen; f++){
												for(int g = f+1; g < tablelen; g++){
													for(int h = g+1; h < tablelen; h++){
														for(int l = h+1; h < tablelen; l++){
															unsigned int mask = (1<<i) | (1<<j) | (1<<k)| (1<<a)| (1<<b)| (1<<c)| (1<<d)| (1<<e)| (1<<f)| (1<<g)| (1<<h)| (1<<l);
															HammingBallMask.push_back(mask);
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

		if(radius>12){
			//radius 13
			for(int i = 0; i < tablelen; i++){
				for(int j = i+1; j < tablelen; j++){
					for(int k = j+1; k < tablelen; k++){
						for(int a = k+1; a < tablelen; a++){
							for(int b = a+1; b < tablelen; b++){
								for(int c = b+1; c < tablelen; c++){
									for(int d = c+1; d < tablelen; d++){
										for(int e = d+1; e < tablelen; e++){
											for(int f = e+1; f < tablelen; f++){
												for(int g = f+1; g < tablelen; g++){
													for(int h = g+1; h < tablelen; h++){
														for(int l = h+1; h < tablelen; l++){
															for(int m = l+1; m < tablelen; m++){
																unsigned int mask = (1<<i) | (1<<j) | (1<<k)| (1<<a)| (1<<b)| (1<<c)| (1<<d)| (1<<e)| (1<<f)| (1<<g)| (1<<h)| (1<<l)| (1<<m);
																HammingBallMask.push_back(mask);
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
			HammingRadius.push_back(HammingBallMask.size());
		}

	}

    void generateMask64(){
      //i = 0 means the origin code
      HammingBallMask64.push_back(0);
      HammingRadius.push_back(HammingBallMask64.size());

      unsigned long One = 1;
      if(radius>0){
        //radius 1
        for(int i = 0; i < tablelen; i++){
          unsigned long mask = One << i;
          HammingBallMask64.push_back(mask);
        }
        HammingRadius.push_back(HammingBallMask64.size());
      }

      if(radius>1){
        //radius 2
        for(int i = 0; i < tablelen; i++){
          for(int j = i+1; j < tablelen; j++){
            unsigned long mask = (One<<i) | (One<<j);
            HammingBallMask64.push_back(mask);
          }
        }
        HammingRadius.push_back(HammingBallMask64.size());
      }

      if(radius>2){
        //radius 3
        for(int i = 0; i < tablelen; i++){
          for(int j = i+1; j < tablelen; j++){
            for(int k = j+1; k < tablelen; k++){
              unsigned long mask = (One<<i) | (One<<j) | (One<<k);
              HammingBallMask64.push_back(mask);
            }
          }
        }
        HammingRadius.push_back(HammingBallMask64.size());
      }

      if(radius>3){
        //radius 4
        for(int i = 0; i < tablelen; i++){
          for(int j = i+1; j < tablelen; j++){
            for(int k = j+1; k < tablelen; k++){
              for(int a = k+1; a < tablelen; a++){
                unsigned long mask = (One<<i) | (One<<j) | (One<<k)| (One<<a);
                HammingBallMask64.push_back(mask);
              }
            }
          }
        }
        HammingRadius.push_back(HammingBallMask64.size());
      }

      if(radius>4){
        //radius 5
        for(int i = 0; i < tablelen; i++){
          for(int j = i+1; j < tablelen; j++){
            for(int k = j+1; k < tablelen; k++){
              for(int a = k+1; a < tablelen; a++){
                for(int b = a+1; b < tablelen; b++){
                  unsigned long mask = (One<<i) | (One<<j) | (One<<k)| (One<<a)| (One<<b);
                  HammingBallMask64.push_back(mask);
                }
              }
            }
          }
        }
        HammingRadius.push_back(HammingBallMask64.size());
      }

      if(radius>5){
        //radius 6
        for(int i = 0; i < tablelen; i++){
          for(int j = i+1; j < tablelen; j++){
            for(int k = j+1; k < tablelen; k++){
              for(int a = k+1; a < tablelen; a++){
                for(int b = a+1; b < tablelen; b++){
                  for(int c = b+1; c < tablelen; c++){
                    unsigned long mask = (One<<i) | (One<<j) | (One<<k)| (One<<a)| (One<<b)| (One<<c);
                    HammingBallMask64.push_back(mask);
                  }
                }
              }
            }
          }
        }
        HammingRadius.push_back(HammingBallMask64.size());
      }

      if(radius>6){
        //radius 7
        for(int i = 0; i < tablelen; i++){
          for(int j = i+1; j < tablelen; j++){
            for(int k = j+1; k < tablelen; k++){
              for(int a = k+1; a < tablelen; a++){
                for(int b = a+1; b < tablelen; b++){
                  for(int c = b+1; c < tablelen; c++){
                    for(int d = c+1; d < tablelen; d++){
                      unsigned long mask = (One<<i) | (One<<j) | (One<<k)| (One<<a)| (One<<b)| (One<<c)| (One<<d);
                      HammingBallMask64.push_back(mask);
                    }
                  }
                }
              }
            }
          }
        }
        HammingRadius.push_back(HammingBallMask64.size());
      }

      if(radius>7){
        //radius 8
        for(int i = 0; i < tablelen; i++){
          for(int j = i+1; j < tablelen; j++){
            for(int k = j+1; k < tablelen; k++){
              for(int a = k+1; a < tablelen; a++){
                for(int b = a+1; b < tablelen; b++){
                  for(int c = b+1; c < tablelen; c++){
                    for(int d = c+1; d < tablelen; d++){
                      for(int e = d+1; e < tablelen; e++){
                        unsigned long mask = (One<<i) | (One<<j) | (One<<k)| (One<<a)| (One<<b)| (One<<c)| (One<<d)| (One<<e);
                        HammingBallMask64.push_back(mask);
                      }
                    }
                  }
                }
              }
            }
          }
        }
        HammingRadius.push_back(HammingBallMask64.size());
      }

      if(radius>8){
        //radius 9
        for(int i = 0; i < tablelen; i++){
          for(int j = i+1; j < tablelen; j++){
            for(int k = j+1; k < tablelen; k++){
              for(int a = k+1; a < tablelen; a++){
                for(int b = a+1; b < tablelen; b++){
                  for(int c = b+1; c < tablelen; c++){
                    for(int d = c+1; d < tablelen; d++){
                      for(int e = d+1; e < tablelen; e++){
                        for(int f = e+1; f < tablelen; f++){
                          unsigned long mask = (One<<i) | (One<<j) | (One<<k)| (One<<a)| (One<<b)| (One<<c)| (One<<d)| (One<<e)| (One<<f);
                          HammingBallMask64.push_back(mask);
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
        HammingRadius.push_back(HammingBallMask64.size());
      }

      if(radius>9){
        //radius 10
        for(int i = 0; i < tablelen; i++){
          for(int j = i+1; j < tablelen; j++){
            for(int k = j+1; k < tablelen; k++){
              for(int a = k+1; a < tablelen; a++){
                for(int b = a+1; b < tablelen; b++){
                  for(int c = b+1; c < tablelen; c++){
                    for(int d = c+1; d < tablelen; d++){
                      for(int e = d+1; e < tablelen; e++){
                        for(int f = e+1; f < tablelen; f++){
                          for(int g = f+1; g < tablelen; g++){
                            unsigned long mask = (One<<i) | (One<<j) | (One<<k)| (One<<a)| (One<<b)| (One<<c)| (One<<d)| (One<<e)| (One<<f)| (One<<g);
                            HammingBallMask64.push_back(mask);
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
        HammingRadius.push_back(HammingBallMask64.size());
      }

      if(radius>10){
        //radius 11
        for(int i = 0; i < tablelen; i++){
          for(int j = i+1; j < tablelen; j++){
            for(int k = j+1; k < tablelen; k++){
              for(int a = k+1; a < tablelen; a++){
                for(int b = a+1; b < tablelen; b++){
                  for(int c = b+1; c < tablelen; c++){
                    for(int d = c+1; d < tablelen; d++){
                      for(int e = d+1; e < tablelen; e++){
                        for(int f = e+1; f < tablelen; f++){
                          for(int g = f+1; g < tablelen; g++){
                            for(int h = g+1; h < tablelen; h++){
                              unsigned long mask = (One<<i) | (One<<j) | (One<<k)| (One<<a)| (One<<b)| (One<<c)| (One<<d)| (One<<e)| (One<<f)| (One<<g)| (One<<h);
                              HammingBallMask64.push_back(mask);
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
        HammingRadius.push_back(HammingBallMask64.size());
      }


    }

	void printCode(Codes& base, unsigned codelength = 32){
		unsigned nTable = base.size();
		std::cout << nTable << " tables:" << std::endl;
		for(unsigned i=0; i<nTable;i++){
			unsigned codetmp = base[i][0];
			std::cout << (codetmp >> (codelength -1)) ;
			for(int j=(codelength-1);j>0;j--){
				codetmp = codetmp & ((1 << j) - 1);
				std::cout << (codetmp >> (j-1)) ;
			}
			std::cout <<  std::endl;
		}
	}

	void printCode64(Codes64& base, unsigned codelength = 64){
		unsigned nTable = base.size();
		std::cout << nTable << " tables:" << std::endl;
		unsigned long One = 1;
		for(unsigned i=0; i<nTable;i++){
			unsigned long codetmp = base[i][0];
			std::cout << (codetmp >> (codelength -1)) ;
			for(int j=(codelength-1);j>0;j--){
				codetmp = codetmp & ((One << j) - 1);
				std::cout << (codetmp >> (j-1));
			}
			std::cout <<  std::endl;
		}
	}

	void printCodeBitset(Codes& base){
		unsigned nTable = base.size();
		for(unsigned i =0; i<nTable;i++){
			unsigned codetmp = base[i][0];
			std::cout << std::bitset<32>(codetmp) << std::endl;
		}
	}

	void printCodeBitset64(Codes64& base){
		unsigned nTable = base.size();
		for(unsigned i =0; i<nTable;i++){
			unsigned long codetmp = base[i][0];
			std::cout << std::bitset<64>(codetmp) << std::endl;
		}
	}


	void buildIndexImpl()
	{
		std::cout<<"HASHING building hashing table"<<std::endl;

		if (tablelen <= 32 ){
			switch(index_method){
			case 0:
				ConvertCode(BaseCodeOrig, BaseCode, tablelen);
				ConvertCode(QueryCodeOrig, QueryCode, tablelen);
				break;
			case 1:
				ConvertCode1(BaseCodeOrig, BaseCode, tablelen);
				ConvertCode1(QueryCodeOrig, QueryCode, tablelen);
				break;
			default:
				std::cout<<"no such indexing method"<<std::endl;
			}


			BuildHashTable32(upbits, tablelen-upbits, BaseCode ,htb);
			generateMask32();

			/*std::cout << "BaseCodeOrig:" << std::endl;
			printCode(BaseCodeOrig);
			std::cout << "BaseCode:" << std::endl;
			printCode(BaseCode,tablelen);*/


		}else{

			ConvertCode64(BaseCodeOrig, BaseCode64, tablelen);
			//std::cout << "BaseCode converted!" << std::endl;
			ConvertCode64(QueryCodeOrig, QueryCode64, tablelen);
			//std::cout << "QueryCode converted!" << std::endl;

			BuildHashTable64(upbits, tablelen-upbits, BaseCode64 ,htb64);
			//std::cout << "hash table built!" << std::endl;
			generateMask64();
			//std::cout << "mask generated!" << std::endl;

			/*std::cout << "BaseCodeOrig:" << std::endl;
			printCode(BaseCodeOrig);
			std::cout << "BaseCode64:" << std::endl;
			printCode64(BaseCode64,tablelen);*/

		}


	}


	void locateNeighbors(const Matrix<DataType>& query){
		int lowbits = tablelen - upbits;

		unsigned int MaxCheck=HammingRadius[radius];
		std::cout<<"maxcheck : "<<MaxCheck<<std::endl;

		boost::dynamic_bitset<> tbflag(features_.get_rows(), false);

		VisitBucketNum.clear();
		VisitBucketNum.resize(radius+2);

		for(size_t cur = 0; cur < query.get_rows(); cur++){

			std::vector<unsigned int> pool(SP.search_init_num);
			unsigned int p = 0;
			tbflag.reset();

			unsigned int j = 0;
			for(; j < MaxCheck; j++){
				for(unsigned int h=0; h < QueryCode.size(); h++){
					unsigned int searchcode = QueryCode[h][cur] ^ HammingBallMask[j];
					unsigned int idx1 = searchcode >> lowbits;
					unsigned int idx2 = searchcode - (idx1 << lowbits);

					HashBucket::iterator bucket= htb[h][idx1].find(idx2);
					if(bucket != htb[h][idx1].end()){
						std::vector<unsigned int> vp = bucket->second;
						for(size_t k = 0; k < vp.size() && p < (unsigned int)SP.search_init_num; k++){
							if(tbflag.test(vp[k]))continue;

							tbflag.set(vp[k]);
							pool[p++]=(vp[k]);
						}
						if(p >= (unsigned int)SP.search_init_num)  break;
					}
					if(p >= (unsigned int)SP.search_init_num)  break;
				}
				if(p >= (unsigned int)SP.search_init_num)  break;
			}

			if(p < (unsigned int)SP.search_init_num){
				VisitBucketNum[radius+1]++;
			}else{
				for(int r=0;r<=radius;r++){
					if(j<=HammingRadius[r]){
						VisitBucketNum[r]++;
						break;
					}
				}
			}
		}
	}


	void getNeighbors(size_t K, const Matrix<DataType>& query){


		//return;

		if(gs.size() != features_.get_rows()){

			if((unsigned)SP.search_init_num < K) {
				std::cout << "# of located points not enough!"<<std::endl;
				return;
			}

			if (tablelen <= 32 ){
				getNeighbors32(K,query);
			}else if(tablelen <= 64 ){
				getNeighbors64(K,query);
			}else{
				std::cout<<"table length not supported yet!"<<std::endl;
			}

		}else{
			switch(SP.search_method){
			case 0:
				getNeighborsIEH32_kgraph(K, query);
				break;
			case 1:
				getNeighborsIEH32_nnexp(K, query);
				break;
			default:
				std::cout<<"no such searching method"<<std::endl;
			}
		}

	}


	void getNeighbors32(size_t K, const Matrix<DataType>& query){


		int lowbits = tablelen - upbits;

		unsigned int MaxCheck=HammingRadius[radius];
		std::cout<<"maxcheck : "<<MaxCheck<<std::endl;

		boost::dynamic_bitset<> tbflag(features_.get_rows(), false);

		nn_results.clear();

		VisitBucketNum.clear();
		VisitBucketNum.resize(radius+2);

		for(size_t cur = 0; cur < query.get_rows(); cur++){

			std::vector<unsigned int> pool(SP.search_init_num);
			unsigned int p = 0;
			tbflag.reset();

			unsigned int j = 0;
			for(; j < MaxCheck; j++){
				for(unsigned int h=0; h < QueryCode.size(); h++){
					unsigned int searchcode = QueryCode[h][cur] ^ HammingBallMask[j];
					unsigned int idx1 = searchcode >> lowbits;
					unsigned int idx2 = searchcode & ((1 << lowbits) - 1);

					HashBucket::iterator bucket= htb[h][idx1].find(idx2);
					if(bucket != htb[h][idx1].end()){
						std::vector<unsigned int> vp = bucket->second;
						for(size_t k = 0; k < vp.size() && p < (unsigned int)SP.search_init_num; k++){
							if(tbflag.test(vp[k]))continue;

							tbflag.set(vp[k]);
							pool[p++]=(vp[k]);
						}
						if(p >= (unsigned int)SP.search_init_num)  break;
					}
					if(p >= (unsigned int)SP.search_init_num)  break;
				}
				if(p >= (unsigned int)SP.search_init_num)  break;
			}

			if(p < (unsigned int)SP.search_init_num){
				VisitBucketNum[radius+1]++;
			}else{
				for(int r=0;r<=radius;r++){
					if(j<=HammingRadius[r]){
						VisitBucketNum[r]++;
						break;
					}
				}
			}

			if (p<K){
				int base_n = features_.get_rows();
				while(p < K){
					unsigned int nn = rand() % base_n;
					if(tbflag.test(nn)) continue;
					tbflag.set(nn);
					pool[p++] = (nn);
				}

			}

			std::vector<int> res;
			if((unsigned)SP.search_init_num == K){
				for(unsigned int j = 0; j < K; j++) res.push_back(pool[j]);
			}else{

				std::vector<std::pair<float,size_t>> result;
				for(unsigned int i=0; i<p;i++){
					result.push_back(std::make_pair(distance_->compare(query.get_row(cur), features_.get_row(pool[i]), features_.get_cols()),pool[i]));
				}
				std::partial_sort(result.begin(), result.begin() + K, result.end());
				for(unsigned int j = 0; j < K; j++) res.push_back(result[j].second);
			}
			nn_results.push_back(res);

		}
	}

	void getNeighbors64(size_t K, const Matrix<DataType>& query){
		int lowbits = tablelen - upbits;

		unsigned int MaxCheck=HammingRadius[radius];
		std::cout<<"maxcheck : "<<MaxCheck<<std::endl;

		boost::dynamic_bitset<> tbflag(features_.get_rows(), false);

		nn_results.clear();

		VisitBucketNum.clear();
		VisitBucketNum.resize(radius+2);


		for(size_t cur = 0; cur < query.get_rows(); cur++){

			std::vector<unsigned int> pool(SP.search_init_num);
			unsigned int p = 0;
			tbflag.reset();

			unsigned int j = 0;
			for(; j < MaxCheck; j++){
				for(unsigned int h=0; h < QueryCode64.size(); h++){
					unsigned long searchcode = QueryCode64[h][cur] ^ HammingBallMask64[j];
					unsigned int idx1 = searchcode >> lowbits;
					unsigned long idx2 = searchcode - (( unsigned long)idx1 << lowbits);

					HashBucket64::iterator bucket= htb64[h][idx1].find(idx2);
					if(bucket != htb64[h][idx1].end()){
						std::vector<unsigned int> vp = bucket->second;
						for(size_t k = 0; k < vp.size() && p < (unsigned int)SP.search_init_num; k++){
							if(tbflag.test(vp[k]))continue;

							tbflag.set(vp[k]);
							pool[p++]=(vp[k]);
						}
						if(p >= (unsigned int)SP.search_init_num)  break;
					}
					if(p >= (unsigned int)SP.search_init_num)  break;
				}
				if(p >= (unsigned int)SP.search_init_num)  break;
			}


			if(p < (unsigned int)SP.search_init_num){
				VisitBucketNum[radius+1]++;
			}else{
				for(int r=0;r<=radius;r++){
					if(j<=HammingRadius[r]){
						VisitBucketNum[r]++;
						break;
					}
				}
			}


			if (p<K){
				int base_n = features_.get_rows();
				while(p < K){
					unsigned int nn = rand() % base_n;
					if(tbflag.test(nn)) continue;
					tbflag.set(nn);
					pool[p++] = (nn);
				}

			}

			std::vector<int> res;
			if((unsigned)SP.search_init_num == K){
				for(unsigned int j = 0; j < K; j++) res.push_back(pool[j]);
			}else{
				std::vector<std::pair<float,size_t>> result;
				for(unsigned int i=0; i<p;i++){
					result.push_back(std::make_pair(distance_->compare(query.get_row(cur), features_.get_row(pool[i]), features_.get_cols()),pool[i]));
				}
				std::partial_sort(result.begin(), result.begin() + K, result.end());
				for(unsigned int j = 0; j < K; j++) res.push_back(result[j].second);
			}
			nn_results.push_back(res);
		}
	}

	void getNeighborsIEH32_nnexp(size_t K, const Matrix<DataType>& query){
		int lowbits = tablelen - upbits;

		unsigned int MaxCheck=HammingRadius[radius];
		std::cout<<"maxcheck : "<<MaxCheck<<std::endl;


		boost::dynamic_bitset<> tbflag(features_.get_rows(), false);
		nn_results.clear();

		VisitBucketNum.clear();
		VisitBucketNum.resize(radius+2);

		for(size_t cur = 0; cur < query.get_rows(); cur++){
			tbflag.reset();

			std::vector<int> pool(SP.search_init_num);
			unsigned int p = 0;


			unsigned int j = 0;
			for(; j < MaxCheck; j++){
				for(size_t h=0; h < QueryCode.size(); h++){
					unsigned int searchcode = QueryCode[h][cur] ^ HammingBallMask[j];
					unsigned int idx1 = searchcode >> lowbits;
					unsigned int idx2 = searchcode & ((1 << lowbits) - 1);

					HashBucket::iterator bucket= htb[h][idx1].find(idx2);
					if(bucket != htb[h][idx1].end()){
						std::vector<unsigned int> vp = bucket->second;
						for(size_t k = 0; k < vp.size() && p < (unsigned int)SP.search_init_num; k++){
							if(tbflag.test(vp[k]))continue;

							tbflag.set(vp[k]);
							pool[p++]=(vp[k]);
						}
						if(p >= (unsigned int)SP.search_init_num)  break;
					}
					if(p >= (unsigned int)SP.search_init_num)  break;
				}
				if(p >= (unsigned int)SP.search_init_num)  break;
			}

			if(p < (unsigned int)SP.search_init_num){
				VisitBucketNum[radius+1]++;
			}else{
				for(int r=0;r<=radius;r++){
					if(j<=HammingRadius[r]){
						VisitBucketNum[r]++;
						break;
					}
				}
			}

			int base_n = features_.get_rows();
			while(p < (unsigned int)SP.search_init_num){
				unsigned int nn = rand() % base_n;
				if(tbflag.test(nn)) continue;
				tbflag.set(nn);
				pool[p++] = (nn);
			}

			InitIndex<DataType>::nnExpansion(K, query.get_row(cur), pool, tbflag);
			nn_results.push_back(pool);
		}
	}

	void getNeighborsIEH32_kgraph(size_t K, const Matrix<DataType>& query){
		int lowbits = tablelen - upbits;

		unsigned int MaxCheck=HammingRadius[radius];
		std::cout<<"maxcheck : "<<MaxCheck<<std::endl;

		nn_results.clear();
		boost::dynamic_bitset<> tbflag(features_.get_rows(), false);

		bool bSorted = true;
		unsigned pool_size = SP.search_epoches * SP.extend_to;
		if (pool_size >= (unsigned)SP.search_init_num){
			SP.search_init_num = pool_size;
			bSorted = false;
		}

		VisitBucketNum.clear();
		VisitBucketNum.resize(radius+2);


		for(size_t cur = 0; cur < query.get_rows(); cur++){

			tbflag.reset();
			std::vector<unsigned int> pool(SP.search_init_num);
			unsigned int p = 0;

			unsigned int j = 0;
			for(; j < MaxCheck; j++){
				for(size_t h=0; h < QueryCode.size(); h++){
					unsigned int searchcode = QueryCode[h][cur] ^ HammingBallMask[j];
					unsigned int idx1 = searchcode >> lowbits;
					unsigned int idx2 = searchcode & ((1 << lowbits) - 1);

					HashBucket::iterator bucket= htb[h][idx1].find(idx2);
					if(bucket != htb[h][idx1].end()){
						std::vector<unsigned int> vp = bucket->second;
						for(size_t k = 0; k < vp.size() && p < (unsigned int)SP.search_init_num; k++){
							if(tbflag.test(vp[k]))continue;

							tbflag.set(vp[k]);
							pool[p++]=(vp[k]);
						}
						if(p >= (unsigned int)SP.search_init_num)  break;
					}
					if(p >= (unsigned int)SP.search_init_num)  break;
				}
				if(p >= (unsigned int)SP.search_init_num)  break;
			}

			if(p < (unsigned int)SP.search_init_num){
				VisitBucketNum[radius+1]++;
			}else{
				for(int r=0;r<=radius;r++){
					if(j<=HammingRadius[r]){
						VisitBucketNum[r]++;
						break;
					}
				}
			}

			int base_n = features_.get_rows();
			while(p < (unsigned int)SP.search_init_num){
				unsigned int nn = rand() % base_n;
				if(tbflag.test(nn)) continue;
				tbflag.set(nn);
				pool[p++] = (nn);
			}

			std::vector<std::pair<float,size_t>> result;
			for(unsigned int i=0; i<pool.size();i++){
				result.push_back(std::make_pair(distance_->compare(query.get_row(cur), features_.get_row(pool[i]), features_.get_cols()),pool[i]));
			}
			if(bSorted){
				std::partial_sort(result.begin(), result.begin() + pool_size, result.end());
				result.resize(pool_size);
			}


			std::vector<int> res;
			InitIndex<DataType>::nnExpansion_kgraph(K,  query.get_row(cur), result, res, bSorted);

			nn_results.push_back(res);

		}
	}

	void outputVisitBucketNum(){
		unsigned i=0;
		std::cout<< "Radius " << i <<" bucket num: "<<HammingRadius[i]<< " points num: "<< VisitBucketNum[i]<<std::endl;

		for(i=1; i<HammingRadius.size();i++){
			std::cout<< "Radius " << i <<" bucket num: "<<HammingRadius[i] - HammingRadius[i-1]<< " points num: "<< VisitBucketNum[i]<<std::endl;
		}
		std::cout<< "Radius larger, points num: " << VisitBucketNum[i]<<std::endl;
	}

	void loadGraph(char* filename){
		std::ifstream in(filename,std::ios::binary);
		in.seekg(0,std::ios::end);
		std::ios::pos_type ss = in.tellg();
		size_t fsize = (size_t)ss;
		int dim;
		in.seekg(0,std::ios::beg);
		in.read((char*)&dim, sizeof(int));
		size_t num = fsize / (dim+1) / 4;
		std::cout<<"load g "<<num<<" "<<dim<< std::endl;
		in.seekg(0,std::ios::beg);
		gs.clear();
		for(size_t i = 0; i < num; i++){
			std::vector<unsigned> heap;
			in.read((char*)&dim, sizeof(int));
			for(int j =0; j < dim; j++){
				unsigned id;
				in.read((char*)&id, sizeof(int));
				heap.push_back(id);
			}
			gs.push_back(heap);
		}
		in.close();
	}

protected:
	USING_BASECLASS_SYMBOLS

	int codelength;
	int tablelen;
	int radius;
	int upbits;
	unsigned index_method = 0;

	Codes BaseCodeOrig;
	Codes QueryCodeOrig;

	Codes BaseCode;
	Codes QueryCode;
	std::vector<HashTable> htb;
	std::vector<unsigned int> HammingBallMask;


	Codes64 BaseCode64;
	Codes64 QueryCode64;
	std::vector<HashTable64> htb64;
	std::vector<unsigned long> HammingBallMask64;


	std::vector<unsigned int> HammingRadius;


	// for statistic info
	std::vector<unsigned int> VisitBucketNum;

};
}
#endif
