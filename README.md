# White-box atomic multicast

This repository contains an implementation of the
[White-Box Atomic Multicast](https://arxiv.org/abs/1904.07171) algorithm.

The implementation is event-driven and rely on [libevent](https://libevent.org)
for asynchronous network communication.

The protocol logic lies in `src/amcast*` and makes use of
[Glib2](https://www.gtk.org/)'s red-black tree and hashmap data structures.

The microbenchmarking application found under the `bench` folder can also drive
[LibMCast](https://bitbucket.org/paulo_coelho/libmcast)'s servers to conduct a
comparative study with other genuine atomic multicast protocols it
implements.

## Build :

    git clone [this repo url]
    make

Depends directly on
[libevent](https://libevent.org),
[Glib2](https://www.gtk.org), and
[LibMCast](https://bitbucket.org/paulo_coelho/libmcast) only for benchmarking
purposes.

## Usage :

Sample usage as a library may be found under `bench/node-bench.c` which
instantiate server or clients on a single machine.

One-time experiments can be run with `bench/runbench_tmux.sh`.

Experiments for a range of clients, destination groups and protocols may be run
with `bench/run_cmpbench.sh`

## Configuration :

Pay attention to environment-specific variables in each bench script.

A cluster configuration read by `bench/node-bench.c` from stdin should be
formatted as a tsv in the following manner:
`NODE_ID\tGROUP_ID\tIP_ADDR\tLISTENER_PORT`,
One line per server and client nodes.

On [Emulab](https://www.emulab.net) and [Cloudlab](https://cloudlab.us)
environment, this may be generated the `utils/emulab_clusterconf_gen.sh`

## License :

Prototype implementation. Use at your own risks.
