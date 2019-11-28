#include "Env.h"
#include <iostream>

void Env::step(float action){
    switch (static_cast<int>(action)) {
        case 1:
            if(state==7) //terminal
            {
                reward += 100;
                done = true;
                if(++_count>1)
                {
                  std::cout<<"reward: "<<reward<<std::endl;
                  _count=0;
                }


            } else
            {
                state += 1;
                reward -= 1;
            }
            break;
        case 0:
            if(state==0) //wall
            {
                state = 0;
                reward -= 1;
            } else
            {
                state -= 1;
                reward -= 1;
            }
            break;
        default:
            break;
    }
}

void Env::rest(){
    state = 0;
    reward = 0;
    done = false;
}
