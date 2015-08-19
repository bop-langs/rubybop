#include "bop_api.h"

#define loop 2
int main(){

  for(int i = 0; i < loop; i++){
    BOP_ppr_begin(1);
      sleep(1);
    BOP_ppr_end(1);
  }
  BOP_group_over(1);

  for(int i = 0; i < loop; i++){
    BOP_ppr_begin(2);
      sleep(1);
    BOP_ppr_end(2);
  }
  BOP_group_over(2);
  return 0;
}
