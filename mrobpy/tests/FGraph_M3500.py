#
import mrob
import numpy as np
import time



# Initialize data structures
vertex_ini = {}
factors = {}
factors_dictionary = {}
N = 3500

# load file
with open('../../benchmarks/M3500.txt', 'r') as file:
    for line in file:
        d = line.split()
        # read edges and vertex, in TORO format
        if d[0] == 'EDGE2':
            # EDGE2 id_origin   id_target   dx   dy   dth   I11   I12  I22  I33  I13  I23
            factors[int(d[1]),int(d[2])] = np.array([d[3], d[4], d[5], d[6],d[7],d[8],d[9],d[10],d[11]],dtype='float64')
            factors_dictionary[int(d[2])].append(int(d[1]))
        else:
            # VERTEX2 id x y theta
            # these are the initial guesses for node states
            vertex_ini[int(d[1])] = np.array([d[2], d[3], d[4]],dtype='float64')
            # create an empty list of pairs of nodes (factor) connected to each node
            factors_dictionary[int(d[1])] = []


# Initialize FG
graph = mrob.fgraph.FGraph()
x = np.zeros(3)
n = graph.add_node_pose_2d(x)
print('node 0 id = ', n) # id starts at 1
graph.add_factor_1pose_2d(x,n,1e9*np.identity(3))
processing_time = []

# start events, we solve for each node, adding it and it corresponding factors
# in total takes 0.3s to read all datastructure
for t in range(1,N):
    x = vertex_ini[t]
    n = graph.add_node_pose_2d(x)
    assert t == n, 'index on node is different from counter'

    # find factors to add. there must be 1 odom and other observations
    connecting_nodes = factors_dictionary[n]

    for nodeOrigin in factors_dictionary[n]:
        # inputs: obs, idOrigin, idTarget, invCov
        obs = factors[nodeOrigin, t][:3]
        covInv = np.zeros((3,3))
        # on M3500 always diagonal information matrices
        covInv[0,0] = factors[nodeOrigin, t][3]
        covInv[1,1] = factors[nodeOrigin, t][5]
        covInv[2,2] = factors[nodeOrigin, t][6]
        graph.add_factor_2poses_2d(obs, nodeOrigin,t,covInv)
        # for end. no more loop inside the factors
        
        
    # solve the problem iteratively 2500nodes
    #graph.solve(mrob.GN)

print('current initial chi2 = ', graph.chi2() )
start = time.time()
graph.solve(mrob.fgraph.LM, 50)
end = time.time()
print('\nLM chi2 = ', graph.chi2() , ', total time on calculation [s] = ', 1e0*(end - start))


