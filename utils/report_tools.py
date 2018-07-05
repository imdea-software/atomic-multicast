import os
import pdb

import matplotlib
#matplotlib.use("Agg")
matplotlib.rcParams['agg.path.chunksize'] = 10000

import pandas as pd
import seaborn as sns
from pandas.tseries.offsets import *
import matplotlib.pyplot as plt

class Experiment:
    columns=["mid", "ts_start", "ts_end", "gts", "ngrp", "destgrps", "pl_l", "pl_v"]

    def __init__(self, exp_msg_count):
        self._stats={}
        self._df={}
        self._total_msg_count=exp_msg_count

    def importDir(self, dir_path):
        if not os.path.isdir(dir_path):
            raise OSError("No such directory")
        for filename in os.listdir(dir_path):
            if filename != "all.clients.log": continue
            if filename.split(".")[0] == "client": continue
            file_path = dir_path + filename
            #TODO Retrieve group id from filename aswell
            node_id = int(filename.split(".")[1]) if filename.split(".")[1] != "clients" else -1
            self._df[node_id] = self._df.setdefault(node_id, pd.DataFrame()).append(pd.read_table(file_path, names=self.columns))
            print(filename, "imported")

    def check(self):
        big_df = pd.concat(self._df, names=["nid"]).drop(columns=["ts_start", "ts_end"]).reset_index(level=["nid"])
        #Check for each node whether the number of messages is OK
        if not big_df.groupby("nid")["mid"].count().eq(self._total_msg_count).all():
                return False
        #Check for each node whether the number of distinct gts is OK
        if not big_df.groupby("nid")["gts"].nunique().eq(self._total_msg_count).all():
                return False
        #Check whether msg data is the same for all nodes involved
        if not big_df.drop("nid", axis=1).groupby("mid").nunique().eq(1).all().all():
            return False
        #TODO Check whether the number of groups is OK
        #TODO Check whether all groups are in destgrp(msg)
        #TODO Check whether the number of nodes for that group is OK
        return True

    #With only one time capture per msg, the stats are not relevent
    def computeStats(self, sampling, stripping, datetime):
        for nid,df in self._df.items():
            #Create a new df from this node data df by droping irrelevent columns and sorting by ts
            stats = self._df[nid].drop(columns=['ngrp','destgrps','pl_l','pl_v'])
            #Add extra delivered column as cumulative number of delivered messages
            #TODO this column should already be in the file if some sampling is already used
            stats.sort_values(by=["gts"]).reset_index(level=0, drop=True, inplace=True)
            #stats.drop(range(12000000, 24000000), inplace=True)
            stats["delivered"] = stats.index
            ts_exp_start = stats["ts_start"].min()
            #Remove records for faster processing
            if sampling:
                stats = stats.sample(n=1000)
            #Add extra time column as offset-ed ts_start for easier reading
            if datetime:
                stats["t"] = pd.to_datetime(stats["ts_start"] - ts_exp_start, unit='s')
            else:
                stats["t"] = stats["ts_start"] - ts_exp_start
            stats.set_index("t", inplace=True)
            #Remove records for faster processing
            if stripping:
                if datetime:
                    stats = stats[ stats.index > pd.Timestamp(ts_exp_start) + 30 * Second()]
                else:
                    stats = stats[ stats.index > 30 ]
            #Add extra latency column as the diff of a row's ts with previous one's ts
            stats["lat"] = stats['ts_end'] - stats['ts_start']
            stats["lat_min"], stats["lat_max"], stats["lat_avg"] = stats["lat"].expanding().min() , stats["lat"].expanding().max(), stats["lat"].expanding().mean()
            #Add extra throughput column as row's index divided by SUM(lat) up to that row
            stats["msgps"] = stats["delivered"] / ( stats["ts_end"] - ts_exp_start )
            #Add this new df to _stats container
            self._stats[nid] = stats
        return

    def altComputeStats(self):
        for nid,df in self._df.items():
            ts_exp_start = self._df[nid]["ts_start"].min()
            ts_exp_softend = self._df[nid]["ts_start"].max()
            ts_exp_end = self._df[nid]["ts_end"].max()
            stats = self._df[nid].drop(columns=['ngrp','destgrps','pl_l','pl_v'])
            stats["delivered"] = stats.sort_values(by=["gts"]).reset_index(level=0, drop=True).index
            stats = stats[ stats["ts_start"] > ts_exp_start + 40 ]
            #stats = stats[ stats["ts_start"] < ts_exp_start + 40 ]
            stats = stats[ stats["ts_start"] < ts_exp_softend - 20 ]
            stats["lat"] = stats['ts_end'] - stats['ts_start']
            stats["msgps"] = stats["delivered"] / ( stats["ts_end"] - ts_exp_start )
            stats["msgps_avg"] = len(stats.index) / ( stats["ts_end"].max() - stats["ts_start"].min() )
            self._stats[nid] = stats
        return


    def plot(self, fig_filename):
        fig, axes = plt.subplots(1, 2, figsize=(20,6))
        for nid,stats in self._stats.items():
            #stats.plot(use_index=True, y=["lat", "lat_avg"], ax=axes[0])
            #stats.plot(use_index=True, y=["msgps"], ax=axes[1])
            stats.plot(use_index=True, y=["lat", "lat_avg"], ax=axes[0], style=['o', 'rx'])
            stats.plot(use_index=True, y=["msgps"], ax=axes[1], style='o')
        axes[0].set_title("Latency (s)")
        axes[1].set_title("Throughput (msg/s)")
        plt.savefig('/users/lefort_a/graphs/' + fig_filename)
        plt.close()
        return

def saveAllGraphsFromExpDirs(dir_path):
    explogpath=dir_path
    for expdir in os.listdir(explogpath):
        _,n_groups,n_nodes,n_targets,n_clients,_,n_msgs,_ = expdir.split(".")
        print("Processing " + expdir)
        exp = Experiment(n_msgs)
        exp.importDir(explogpath + expdir + "/")
        #if not exp.check():
        #    raise Exception("FAILURE: report files are not consistent")
        exp.computeStats(False, False, True)
        exp.plot(expdir + ".full" + ".png")
        exp.computeStats(False, False, False)
        exp.plot(expdir + ".full.nodatetime" + ".png")
        exp.computeStats(True, False, True)
        exp.plot(expdir + ".sampled" + ".png")
        exp.computeStats(False, True, True)
        exp.plot(expdir + ".stripped" + ".png")
        exp.computeStats(True, True, True)
        exp.plot(expdir + ".sampled.stripped" + ".png")
        exp.computeStats(False, True, False)
        exp.plot(expdir + ".stripped.nodatetime" + ".png")
        print("Done " + expdir)

def prepareAggStats():
    proto = ('amcast', 'basecast')
    groups = range(6)
    labels = ('1G/2000C', '2G/2000C', '3G/1000C', '4G/1000C', '5G/500C', '6G/500C')
    #agg_stats = pd.DataFrame(index=pd.MultiIndex.from_product([proto, groups], names=['Protocol', 'Config']))
    agg_stats = pd.DataFrame(columns=['Protocol', 'Config', 'lat', 'msgps'])
    return agg_stats

def computeAggStats(dir_path, agg_stats, proto):
    labels = ('1G/2000C', '2G/2000C', '3G/1000C', '4G/1000C', '5G/500C', '6G/500C')
    for expdir in os.listdir(dir_path):
        print("Processing " + expdir)
        _,n_groups,n_nodes,n_targets,n_clients,_,n_msgs,_ = expdir.split(".")
        exp = Experiment(n_msgs)
        exp.importDir(explogpath + expdir + "/")
        #if not exp.check():
        #    raise Exception("FAILURE: report files are not consistent")
        exp.altComputeStats()
        exp_agg_stats = exp._stats[-1].loc[:, ['lat', 'msgps', 'msgps_avg']]
        exp_agg_stats.loc[:, "Protocol"] = proto
        #exp_agg_stats.loc[:, "Config"] = labels[int(n_targets)-1]
        exp_agg_stats.loc[:, "Config"] = n_targets
        agg_stats = agg_stats.append(exp_agg_stats, ignore_index=True)
        #agg_stats.loc[(proto,n_targets), 'lat'] = exp._stats[-1]["lat"]
        #agg_stats.loc[(proto,n_targets), 'msgps'] = exp._stats[-1]["msgps"]
    return agg_stats

def plotAggStatsBox(agg_stats, fig_filename):
    fig, axes = plt.subplots(1, 3, figsize=(30,6))
    plot_type = fig_filename.split(".")[0]
    sns.boxplot(x='Config', y='lat', hue='Protocol', data=agg_stats, ax=axes[0])
    sns.boxplot(x='Config', y='msgps', hue='Protocol', data=agg_stats, ax=axes[1])
    sns.boxplot(x='Config', y='msgps_avg', hue='Protocol', data=agg_stats, ax=axes[2])
    axes[0].set_title("Latency (s)")
    axes[1].set_title("Throughput (msg/s)")
    axes[2].set_title("Throughput avg (msg/s)")
    plt.savefig('/users/lefort_a/graphs/' + fig_filename)
    plt.close()
    return

def plotAggStatsBar(agg_stats, fig_filename):
    fig, axes = plt.subplots(1, 3, figsize=(30,6))
    plot_type = fig_filename.split(".")[0]
    sns.barplot(x='Config', y='lat', hue='Protocol', data=agg_stats, ax=axes[0])
    sns.barplot(x='Config', y='msgps', hue='Protocol', data=agg_stats, ax=axes[1])
    sns.barplot(x='Config', y='msgps_avg', hue='Protocol', data=agg_stats, ax=axes[2])
    axes[0].set_title("Latency (s)")
    axes[1].set_title("Throughput (msg/s)")
    axes[2].set_title("Throughput avg (msg/s)")
    plt.savefig('/users/lefort_a/graphs/' + fig_filename)
    plt.close()
    return




if __name__ == "__main__":

    agg_stats = prepareAggStats()

    explogpath="/proj/RDMA-RCU/tmp/amcast/"
    #saveAllGraphsFromExpDirs(explogpath)
    agg_stats = computeAggStats(explogpath, agg_stats, 'amcast')

    explogpath="/proj/RDMA-RCU/tmp/mcast/"
    #saveAllGraphsFromExpDirs(explogpath)
    agg_stats = computeAggStats(explogpath, agg_stats, 'basecast')

    plotAggStatsBox(agg_stats, "box.aggview.png")
    plotAggStatsBar(agg_stats, "bar.aggview.png")

    #pdb.set_trace()
