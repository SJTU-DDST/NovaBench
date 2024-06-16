#include<iostream>
#include<string>

using namespace std;

int main(){
    std::cout << "config-0" << std::endl;
    std::cout << "0,1" << std::endl;
    std::cout << "0,1" << std::endl;
    std::cout << "0" << std::endl;

    int64_t range = 10000000;
    int64_t part = range / 256;



    for(int i = 0; i < 256; i++){
        std::cout << i * part << ",";
        if(i == 255){
            std::cout << range << ",";
        } else{
            std::cout << (i + 1) * part << ",";
        }

        if(i < 128){
            std::cout << 0 << "," << i << std::endl;
        }else{
            std::cout << 1 << "," << i << std::endl;
        }

        
    }
    return 0;
}