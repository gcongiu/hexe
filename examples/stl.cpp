#include <vector>
#include <iostream>
#include "hexe_allocator.hpp"

int main(int argc, char** argv){


  std::vector<int, hexe::HexeAlloc<int> > coefs_int;
  std::vector<double, hexe::HexeAlloc<double> > coefs_double;
  std::vector<long, hexe::HexeAlloc<long> > coefs_long;


  hexe_reserve( 3, coefs_int, 1024*1024*1024);
  hexe_reserve( 10, coefs_double, 1024*1024*1024);
  hexe_reserve( 12, coefs_long, 1024*1024*1024);
  hexe_distribute_objects
//  coefs[0] = 3;

  std::cout<<"Coefs int is in ? "<<  (hexe_verify(coefs_int) == 0  ? "yes" : "no")  <<std::endl;
  std::cout<<"Coefs double is in ? "<< (hexe_verify(coefs_double) == 0 ? "yes" : "no") <<std::endl;
  std::cout<<"Coefs long is in ? "<<( hexe_verify(coefs_long) == 0 ? "yes" :"no") <<std::endl;
    return 0;
}
