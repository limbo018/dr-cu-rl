##
# @file   Envs.py
# @author Yibo Lin
# @date   Feb 2020
#

import numpy as np 
import gym 
from gym import spaces 
import drcu_cpp 

class DrcuEnv (gym.Env):
    def __init__(self, args):
        super(DrcuEnv, self).__init__()
        self.drcu = drcu_cpp.Drcu()
        self.drcu.init(args)
        # feature shape is #nets x #features 
        feature = np.array(self.drcu.get_the_1st_observation())
        #reward = 0 
        #done = False 

        self.action_space = spaces.Box(low=0, high=1, shape=(1,), dtype=np.float32)
        self.observation_space = spaces.Box(low=0, high=1, shape=feature.shape, dtype=np.float32)

    def step(self, action):
        res = self.drcu.step(action)
        return {'feature' : res.feature, 'reward' : res.reward, 'done' : res.done}

    def reset(self):
        self.drcu.reset()

        feature = np.array(self.drcu.get_the_1st_observation())
        reward = 0 
        done = False 
        return {'feature' : feature, 'reward' : reward, 'done' : done}

    def render(self, observation=None):
        vio = self.drcu.get_all_vio() 
        content = "vio: %g, %g, %g, %g" % (vio[0], vio[1], vio[2], vio[3])
        if observation: 
            content += " ; " + str(observation)
        print(content)
