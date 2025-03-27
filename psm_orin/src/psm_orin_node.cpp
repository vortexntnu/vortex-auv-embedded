#include "psm_orin_node.hpp"
#include "psm_orin_driver.h"

PSMOrinNode::PSMOrinNode() {
    set_subscribers_and_publishers();
    i2c_psm_init();

}

void PSMOrinNode::publish_voltage(){
  double voltage = -1;
  double current = -1;
  if(read_scaled_measurements(&voltage, &current)){

  }
  

}

void PSMOrinNode::publish_current(){

}
