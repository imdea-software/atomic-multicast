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
    columns=["mid", "ts_start", "ts_end", "gts", "ngrp", "destgrps", "pl_l", "pl_v", "q_size"]
    dtypes={'mid': str, 'ts_start': float, 'ts_end': float, 'gts': str, 'ngrp': float, 'destgrps': str, 'pl_l': float, 'pl_v': str, 'q_size': float}
    #columns=["mid", "ts_start", "ts_end", "gts", "ngrp", "destgrps", "pl_l", "pl_v"]
    #dtypes={'mid': str, 'ts_start': float, 'ts_end': float, 'gts': str, 'ngrp': float, 'destgrps': str, 'pl_l': float, 'pl_v': str}

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
            self._df[node_id] = self._df.setdefault(node_id, pd.DataFrame()).append(pd.read_table(file_path, names=self.columns, dtype=self.dtypes))
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
            if nid not in [-1, 0, 3, 6, 9, 12, 15]: continue
            stats = self._df[nid].drop(columns=['ngrp','destgrps','pl_l','pl_v'])
            #stats['mid'] = stats['mid'].apply(lambda x: eval(x))
            #stats[['smid', 'cid']] = stats['mid'].apply(pd.Series)
            stats['smid'] = stats['mid'].apply(lambda x: int(x.split(',')[0][1:]))
            stats['cid'] = stats['mid'].apply(lambda x: int(x.split(',')[1][:-1]))
            stats.drop(columns=['mid'], inplace=True)
            ts_exp_rstart = stats["ts_start"].min()
            ts_exp_start = stats.groupby('cid')["ts_start"].min().max()
            ts_exp_softend = stats.groupby('cid')["ts_start"].max().min()
            ts_exp_end = stats.groupby('cid')["ts_end"].max().min()
            #ts_exp_start = self._df[nid]["ts_start"].min()
            #ts_exp_softend = self._df[nid]["ts_start"].max()
            #ts_exp_end = self._df[nid]["ts_end"].max()
            #stats = self._df[nid].drop(columns=['ngrp','destgrps','pl_l','pl_v'])
            stats["delivered"] = stats.sort_values(by=["gts"]).reset_index(level=0, drop=True).index
            stats = stats[ stats["ts_start"] > ts_exp_start + 20 ]
            stats = stats[ stats["ts_start"] < ts_exp_softend - 20 ]
            if len(stats.index) == 0:
                #return False
                continue
            #stats = stats[ stats["ts_start"] < ts_exp_start + 40 ]
            stats["t"] = pd.to_datetime(stats['ts_end'] - ts_exp_rstart, unit='s')
            stats["lat"] = stats['ts_end'] - stats['ts_start']
            stats["lat_avg"] = stats["lat"].mean()
            stats["msgps"] = stats["delivered"] / ( stats["ts_end"] - stats["ts_start"].min() )
            stats["msgps_avg"] = len(stats.index) / ( stats["ts_end"].max() - stats["ts_start"].min() )
            stats["q_size_avg"] = stats["q_size"].mean()
            self._stats[nid] = stats
        return True


    def plot(self, fig_filename):
        for nid,stats in self._stats.items():
            fig, axes = plt.subplots(3, 2, figsize=(60,10))
            #stats.plot(use_index=True, y=["lat", "lat_avg"], ax=axes[0])
            #stats.plot(use_index=True, y=["msgps"], ax=axes[1])
            stats.plot(x="t", y=["lat", "lat_avg"], ax=axes[1][0])
            stats.plot(x="t", y=["msgps", "msgps_avg"], ax=axes[1][1])
            #stats.plot(x="t", y=["q_size", "q_size_avg"], ax=axes[2])
            stats.resample('1S', on='t')['lat'].mean()[:-1].plot(use_index=True, ax=axes[0][0])
            stats.resample('1S', on='t')['lat_avg'].mean()[:-1].plot(use_index=True, ax=axes[0][0])
            stats.resample('1S', on='t')['msgps'].count()[:-1].plot(use_index=True, ax=axes[0][1])
            stats.resample('1S', on='t')['msgps_avg'].mean()[:-1].plot(use_index=True, ax=axes[0][1])
            sns.distplot(stats["lat"], ax=axes[2][0])
            sns.distplot(stats["msgps"], ax=axes[2][1])
            #stats.resample('1S', on='t')['q_size'].max()[:-1].plot(use_index=True, ax=axes[2])
            #stats.plot(x='t', y='q_size', ax=axes[3])
            axes[0][0].set_title("Latency (s)")
            axes[0][1].set_title("Throughput (msg/s)")
            #failures=[70, 105, 139, 174]
            #for i in range(2):
            #    for failure in failures:
            #        axes[i].axvline(x=failure)
            #axes[2].set_title("Delivery Queue size (msg) sampled")
            #axes[3].set_title("Delivery Queue size (msg) raw")
            plt.savefig('/users/lefort_a/graphs/node_' + str(nid) + fig_filename)
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

def prepareCmpStats():
    proto = ('amcast', 'basecast')
    groups = range(6)
    labels = ('1G', '2G', '3G', '4G', '5G', '6G')
    cmp_stats = pd.DataFrame(columns=['Protocol', 'Config', 'Clients', 'lat_avg', 'msgps_avg'])
    return cmp_stats

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

def computeCmpStats(dir_path, cmp_stats, proto):
    labels = ('1G', '2G', '3G', '4G', '5G', '6G')
    for expdir in os.listdir(dir_path):
        _,n_groups,n_nodes,n_targets,n_clients,n_client_hosts,n_msgs,_ = expdir.split(".")
        #if n_targets not in ["2","3"]: continue
        print("Processing " + expdir)
        exp = Experiment(n_msgs)
        exp.importDir(explogpath + expdir + "/")
        #if not exp.check():
        #    raise Exception("FAILURE: report files are not consistent")
        if not exp.altComputeStats():
            continue
        #exp.plot(expdir + ".dist." + proto + ".png")
        #nid=-1
        for nid,stats in exp._stats.items():
            exp_cmp_stats = pd.DataFrame({"Protocol": proto, "Config": n_targets, "Clients": n_clients, "Node": nid, "lat_avg": exp._stats[nid]["lat"].mean(), "msgps_avg": exp._stats[nid]["msgps_avg"].mean(), "Delivered": len(exp._stats[nid].index), "Duration": exp._stats[nid]['ts_end'].max() - exp._stats[nid]['ts_start'].min()}, index=range(1))
            cmp_stats = cmp_stats.append(exp_cmp_stats, ignore_index=True)
        #agg_stats.loc[(proto,n_targets), 'lat'] = exp._stats[-1]["lat"]
        #agg_stats.loc[(proto,n_targets), 'msgps'] = exp._stats[-1]["msgps"]
    return cmp_stats

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

def plotCmpStatsLine(agg_stats, fig_filename):
    dests=[1,2,3,4,6,8,10]
    fig, axes = plt.subplots(2, len(dests), figsize=(30,len(dests)))
    plot_type = fig_filename.split(".")[0]
    for i in range(len(dests)):
        agg_stats = agg_stats[agg_stats['Node'] == -1]
        #data = agg_stats[agg_stats['Config'] == str(i+1)]
        data = agg_stats[agg_stats['Config'] == dests[i]]
        data = data.sort_values(by=['Clients','Protocol'])
        #data1 = agg_stats[agg_stats['Config'] == 2]
        #data2 = agg_stats[agg_stats['Config'] == 4]
        #data = agg_stats[agg_stats['Config'] == 2]
        nodes=[-1, 0, 3, 6, 9, 12, 15]
        #amcast_data = data[data['Protocol'] == 'amcast']
        #basecast_data = data[data['Protocol'] == 'basecast']
        sns.lineplot(x='lat_avg', y='msgps_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=data, sort=False, ax=axes[0][i])
        sns.lineplot(x='lat_avg', y='msgps_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=data, sort=False, ax=axes[1][i])
        #sns.lineplot(x='Clients', y='lat_avg', style='Config', hue='Config', markers=True, dashes=False, data=data[data['Node'] == nodes[i]], ax=axes[i])
        #sns.lineplot(x='Clients', y='lat_avg', hue='Node', style='Node', markers=True, dashes=False, data=data[data['Node'] == -1], ax=axes[0][i])
        #sns.lineplot(x='Clients', y='lat_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=data1[data1['Node'] == nodes[i]], ax=axes[0][i])
        #sns.lineplot(x='Clients', y='lat_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=data2[data2['Node'] == nodes[i]], ax=axes[1][i])
        #sns.lineplot(x='lat_avg', y='msgps_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=amcast_data, ax=axes[1][i])
        #sns.lineplot(x='lat_avg', y='msgps_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=basecast_data, ax=axes[2][i])
        axes[0][i].set_title(str(dests[i]) + " destination groups")
    #axes[0].set_title(" 2 destination groups")
    #axes[1].set_title(" 4 destination groups")
        #axes[0][i].set_title("Node " + str(nodes[i]))
        #axes[1][i].set_title("Node " + str(nodes[i]))
        for idx,row in data.iterrows():
            axes[1][i].annotate(row['Clients'], (row['lat_avg'], row['msgps_avg']))
    plt.savefig('/mnt/graphs/' + fig_filename)
    plt.close()
    return

if __name__ == "__main__":
    cmp_stats = prepareCmpStats()
    explogpath="/mnt/lan_10ch_f1proxy_amcast/"
    cmp_stats = computeCmpStats(explogpath, cmp_stats, 'amcast')
    explogpath="/mnt/lan_10ch_f1proxy_basecast/"
    cmp_stats = computeCmpStats(explogpath, cmp_stats, 'basecast')
    explogpath="/mnt/lan_10ch_f1proxy_famcast/"
    cmp_stats = computeCmpStats(explogpath, cmp_stats, 'famcast')
    #explogpath="/mnt/vwan_6ch_proxy17_amcast/"
    #cmp_stats = computeCmpStats(explogpath, cmp_stats, 'proxytc9_amcast')
    #for i in range(2,8):
    #    explogpath="/mnt/vwan_6ch_proxy" + str(i) + "_amcast/"
    #    cmp_stats = computeCmpStats(explogpath, cmp_stats, 'proxy_amcast')
    exported_df_path="/mnt/dataframe/lan_10ch_f1proxy_dataframe.csv"
    cmp_stats.to_csv(exported_df_path, sep='\t')
    #cmp_stats = pd.read_table(exported_df_path, sep='\t', index_col=0)
    #cmp_stats = ocmp_stats[ocmp_stats['Protocol'] != 'amcast']

    #cmp_stats = cmp_stats.groupby(["Clients","Config"]).mean().reset_index()
    #cmp_stats["Protocol"] = 'proxy_amcast'

    #plotCmpStatsLine(cmp_stats, "line.uniform.10ch.lan2.png")
"""
    #pdb.set_trace()
"""
"""
    agg_stats = prepareAggStats()

    explogpath="/proj/RDMA-RCU/tmp/amcast/"
    #saveAllGraphsFromExpDirs(explogpath)
    agg_stats = computeAggStats(explogpath, agg_stats, 'amcast')

    explogpath="/proj/RDMA-RCU/tmp/mcast/"
    #saveAllGraphsFromExpDirs(explogpath)
    agg_stats = computeAggStats(explogpath, agg_stats, 'basecast')

    plotAggStatsBox(agg_stats, "box.aggview.png")
    plotAggStatsBar(agg_stats, "bar.aggview.png")
"""
