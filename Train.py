##
# @file   Train.py
# @author Yibo Lin
# @date   Feb 2020
#

import sys
import numpy as np 
import Envs 
import pdb 

print(sys.argv)
env = Envs.DrcuEnv(sys.argv)
env.render()
print("reset")
observation = env.reset()
env.render()
print("step")
action = np.random.uniform(0, 1, size=env.observation_space.shape[0])
observation = env.step(action)
env.render(observation)
