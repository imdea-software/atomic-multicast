import os
import pdb
import math

import numpy as np
import matplotlib
#matplotlib.use("Agg")
matplotlib.rcParams['agg.path.chunksize'] = 10000

import pandas as pd
import seaborn as sns
from pandas.tseries.offsets import *
import matplotlib.pyplot as plt


class Experiment:
    columns=["mid", "ts_start", "ts_commit", "ts_end", "gts", "ngrp", "destgrps", "pl_l", "pl_v", "q_size"]
    dtypes={'mid': str, 'ts_start': float, 'ts_commit': float, 'ts_end': float, 'gts': str, 'ngrp': int, 'destgrps': str, 'pl_l': int, 'pl_v': str, 'q_size': int}
    #columns=["mid", "ts_start", "ts_end", "gts", "ngrp", "destgrps", "pl_l", "pl_v"]
    #dtypes={'mid': str, 'ts_start': float, 'ts_end': float, 'gts': str, 'ngrp': float, 'destgrps': str, 'pl_l': float, 'pl_v': str}

    def __init__(self, exp_msg_count):
        self._gstats={}
        self._stats={}
        self._df={}
        self._events={}
        self._recovery={}
        self._total_msg_count=exp_msg_count

    def importDir(self, dir_path):
        if not os.path.isdir(dir_path):
            raise OSError("No such directory")
        for filename in os.listdir(dir_path):
            #if filename != "all.clients.log": continue
            if filename.split(".")[0] == "client": continue
            file_path = dir_path + filename
            if filename == "events.log":
                self._events = pd.DataFrame().append(pd.read_csv(file_path, names=["node", "pleader", "ts_start", "ts_end", "nb_msg"], sep=' '))
                continue
            #TODO Retrieve group id from filename aswell
            node_id = int(filename.split(".")[1]) if filename.split(".")[1] != "clients" else -1
            #if node_id not in [-1, 0, 3, 6, 9, 10, 11, 12, 15, 18, 21, 24, 27]: continue
            #if node_id not in [-1, 0, 1, 2, 9, 10, 11]: continue
            self._df[node_id] = self._df.setdefault(node_id, pd.DataFrame()).append(pd.read_csv(file_path, names=self.columns, dtype=self.dtypes, sep="\t"))
            #self._df[node_id] = self._df.setdefault(node_id, pd.DataFrame()).append(pd.read_csv(file_path, names=self.columns, dtype=self.dtypes, sep="\t", nrows=100000000))
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
            ##TODO this column should already be in the file if some sampling is already used
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
            #if nid not in [-1, 0, 3, 6, 9, 12, 15]: continue
            stats = self._df[nid].drop(columns=['ngrp','destgrps','pl_l','pl_v'])

            if nid in [-1]:
                stats['smid'] = stats['mid'].apply(lambda x: int(x.split(',')[0][1:]))
                stats['cid'] = stats['mid'].apply(lambda x: int(x.split(',')[1][:-1]))
                stats[["ts_start", "ts_end"]] = stats.groupby("cid")[["ts_start","ts_end"]].apply(lambda x: x - x["ts_start"].min())

            ts_exp_rstart = stats["ts_start"].min()
            stats = stats[ stats["ts_start"] >= ts_exp_rstart + 15 ]
            stats = stats[ stats["ts_end"] < ts_exp_rstart + 90 ]
            ts_exp_end = stats["ts_end"].max()
            stats = stats[ stats["ts_end"] < ts_exp_end - 15 ]

            #stats['mid'] = stats['mid'].apply(lambda x: eval(x))
            #stats[['smid', 'cid']] = stats['mid'].apply(pd.Series)

            #stats['smid'] = stats['mid'].apply(lambda x: int(x.split(',')[0][1:]))
            #stats['cid'] = stats['mid'].apply(lambda x: int(x.split(',')[1][:-1]))
            #stats.drop(columns=['mid'], inplace=True)

            #ts_exp_end = stats.groupby('cid')["ts_end"].max().min()
            #ts_exp_start = stats.groupby('cid')["ts_start"].min().max()

            #ts_exp_softend = ddstats.groupby('cid')["ts_start"].max().min()
            #ts_exp_end = stats.groupby('cid')["ts_end"].max().min()
            #ts_exp_start = self._df[nid]["ts_start"].min()
            #ts_exp_softend = self._df[nid]["ts_start"].max()
            #ts_exp_end = self._df[nid]["ts_end"].max()
            #stats = self._df[nid].drop(columns=['ngrp','destgrps','pl_l','pl_v'])

            #stats = stats[ stats["ts_start"] > ts_exp_start ]
            #stats = stats[ stats["ts_end"] < ts_exp_end ]
            if len(stats.index) == 0:
                print("Empty experiment")
                #return False
                continue
            stats["delivered"] = stats.sort_values(by=["gts"]).reset_index(level=0, drop=True).index
            #stats = stats[ stats["ts_start"] < ts_exp_start + 40 ]
            stats["t"] = pd.to_datetime(stats['ts_end'] - ts_exp_rstart, unit='s')
            stats["lat"] = stats['ts_end'] - stats['ts_start']
            stats["lat_avg"] = stats["lat"].mean()
            stats["msgps"] = stats["delivered"] / ( stats["ts_end"] - stats["ts_start"].min() )
            stats["msgps_avg"] = len(stats.index) / ( stats["ts_end"].max() - stats["ts_start"].min() )
            stats["q_size_avg"] = stats["q_size"].mean()
            self._stats[nid] = stats
        return True

    def recComputeStats(self):
        for nid,df in self._df.items():
            #Remove unwanted data
            stats = self._df[nid].drop(columns=['ngrp','destgrps','pl_l','pl_v','q_size'])
            #Normalize client timestamps
            if nid in [-1]:
                stats['smid'] = stats['mid'].apply(lambda x: int(x.split(',')[0][1:]))
                stats['cid'] = stats['mid'].apply(lambda x: int(x.split(',')[1][:-1]))
                stats['gid'] = stats['cid'].apply(lambda x: (x-1) * 9 // 6000 if x in [2000, 4000, 6000] else x * 9 // 6000)
                stats[["ts_start", "ts_commit", "ts_end"]] = stats.groupby("gid")[["ts_start", "ts_commit", "ts_end"]].apply(lambda x: x - x["ts_start"].min())
            #Cut head and tail of experiment
            ts_exp_rstart = stats["ts_start"].min()
            #stats = stats[ stats["ts_start"] >= ts_exp_rstart + 90 ]
            #stats = stats[ stats["ts_start"] >= ts_exp_rstart + 105 ]
            stats = stats[ stats["ts_start"] >= ts_exp_rstart + 50 ]
            stats = stats[ stats["ts_end"] < ts_exp_rstart + 180 ]
            if nid not in [9]:
                ts_exp_end = stats["ts_end"].max()
                stats = stats[ stats["ts_end"] < ts_exp_end - 90 ]
            #    stats = stats[ stats["ts_end"] < ts_exp_end - 50 ]
            #Skip if empty
            if len(stats.index) == 0:
                print("Empty experiment")
                continue
            if nid in [10,11]:
                recovery = self._events[self._events["node"] == nid].drop(columns=["node"])
                recovery["ts_start"] = pd.to_datetime(recovery["ts_start"] - ts_exp_rstart, unit='s')
                recovery["ts_end"] = pd.to_datetime(recovery["ts_end"] - ts_exp_rstart, unit='s')
                self._recovery[nid] = self._recovery.setdefault(nid, pd.DataFrame()).append(recovery)
            #Re-order
            #stats["delivered"] = stats.sort_values(by=["gts"]).reset_index(level=0, drop=True).index
            #Compute Stats
            stats["t"] = pd.to_datetime(stats['ts_end'] - ts_exp_rstart, unit='s')
            stats["lat"] = stats['ts_end'] - stats['ts_start']
            stats["clat"] = stats['ts_commit'] - stats['ts_start']
            #stats["lat_avg"] = stats["lat"].mean()
            #stats["msgps"] = stats["delivered"] / ( stats["ts_end"] - stats["ts_start"].min() )
            #stats["msgps_avg"] = len(stats.index) / ( stats["ts_end"].max() - stats["ts_start"].min() )
            #stats["q_size_avg"] = stats["q_size"].mean()
            #self._stats[nid] = stats
            if nid in [-1]: gid = -1
            else:
                gid = nid // 3
            self._gstats[gid] = self._gstats.setdefault(gid, pd.DataFrame()).append(stats)
        return True

    def recPlot(self, fig_filename):
        sns.set_style("whitegrid")
        sns.set_context("paper")
        fig, axes = plt.subplots(2, figsize=(7.5,5))
        lat,msgps = pd.DataFrame(),pd.DataFrame()
        for gid,stats in self._gstats.items():
            stats.drop_duplicates(subset=['mid'], keep='first', inplace=True)
            sampled = stats.resample('0.3S', on='t')
            column = "clients" if gid == -1 else "group " + str( gid )
            lat[column] = sampled["lat"].mean().apply(lambda x: x)[1:-1]
            #commit[column] = sampled["clat"].mean().apply(lambda x: x * 1000)[1:-1]
            msgps[column] = sampled["lat"].count().apply(lambda x: x / 0.3 / 1000)[1:-1]
        sns.lineplot(data=lat, markers=False, dashes=False, ax=axes[0], palette="deep", legend="brief")
        #sns.lineplot(data=commit, markers=False, dashes=False, ax=axes[1], palette="deep", legend=False)
        sns.lineplot(data=msgps, markers=False, dashes=False, ax=axes[1], palette="deep", legend=False)
        for nid,recovery in self._recovery.items():
            if nid == 11: continue
            color='r' if nid == 10 else 'g'
            for axe in axes:
                axe.axvspan(recovery["ts_start"].min(), recovery["ts_end"].min(), facecolor=color, alpha=0.5)
        sns.despine()
        axes[0].set(xlabel="Time [sec]", ylabel="Latency [sec]")
        #axes[1].set(xlabel="Time [sec]", ylabel="Commit Latency [msec]")
        axes[1].set(xlabel="Time [sec]", ylabel="Throughput [k-messages / sec]")
        plt.tight_layout()
        plt.savefig('/home/anatole/graphs/' + fig_filename,bbox_inches='tight')
        plt.close()
        return


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
            #plt.savefig('/users/lefort_a/graphs/node_' + str(nid) + fig_filename)
            plt.savefig('/home/anatole/graphs/node_' + str(nid) + fig_filename)
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

def computeCmpStats(dir_path, df_path, cmp_stats, proto):
    labels = ('1G', '2G', '3G', '4G', '5G', '6G')
    for expdir in os.listdir(dir_path):
        _,n_groups,n_nodes,n_targets,n_clients,n_client_hosts,n_msgs,_ = expdir.split(".")
        #if n_targets not in ["1"]: continue
        if not cmp_stats[cmp_stats["Protocol"] == proto][cmp_stats["Config"] == int(n_targets)][cmp_stats["Clients"] == int(n_clients)].empty: continue
        print("Processing " + proto + " " + expdir)
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
        exp = None
        cmp_stats.to_csv(df_path, sep='\t')
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
    sns.set_style("whitegrid")
    sns.set_context("paper")
    #dests=[1,2,3,4,6,8,10]
    #dests=[1,2,4,6,8,10]
    dests=[2,4,8]
    #fig, axes = plt.subplots(1, len(dests), figsize=(12*5,1*5))
    #fig = plt.figure(figsize=(3.4*1.5,6*1.5))
    #fig = plt.figure(figsize=(2*5,1*5))
    fig = plt.figure(figsize=(2*5,0.5*5))
    plot_type = fig_filename.split(".")[0]
    for i in range(len(dests)):
        #axe = fig.add_subplot(4,2,i+1)
        axe = fig.add_subplot(1,3,i+1)
        #axe = axes[i // 2][i % 2]
        legend = "brief" if i == 0 else False
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
        sns.lineplot(x='lat_avg', y='msgps_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=data, sort=False, ax=axe, palette="deep", legend=legend)
        if i == 0:
            handles, labels = axe.get_legend_handles_labels()
            axe.legend(handles=handles[1:], labels=labels[1:])
        """
        axe.set(ylabel="", xlabel="")
        if i % 2 == 0:
            axe.set(ylabel="Throughput [messages / sec]", xlabel="")
        if i // 2 >= 3:
            axe.set(xlabel="Latency [msec]", ylabel="")
        if i % 2 == 0 and i // 2 >= 3:
        """
        axe.set(xlabel="Latency [msec]", ylabel="Throughput [k-messages / sec]")
        """
        if i == 0:
            axe.set(xlabel="Latency [msec]", ylabel="Throughput [messages / sec]")
        else:
            axe.set(xlabel="Latency [msec]", ylabel="")
        """
        sns.despine()
        #sns.lineplot(x='lat_avg', y='msgps_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=data, sort=False, ax=axes[1][i])
        #sns.lineplot(x='Clients', y='lat_avg', style='Config', hue='Config', markers=True, dashes=False, data=data[data['Node'] == nodes[i]], ax=axes[i])
        #sns.lineplot(x='Clients', y='lat_avg', hue='Node', style='Node', markers=True, dashes=False, data=data[data['Node'] == -1], ax=axes[0][i])
        #sns.lineplot(x='Clients', y='lat_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=data1[data1['Node'] == nodes[i]], ax=axes[0][i])
        #sns.lineplot(x='Clients', y='lat_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=data2[data2['Node'] == nodes[i]], ax=axes[1][i])
        #sns.lineplot(x='lat_avg', y='msgps_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=amcast_data, ax=axes[1][i])
        #sns.lineplot(x='lat_avg', y='msgps_avg', hue='Protocol', style='Protocol', markers=True, dashes=False, data=basecast_data, ax=axes[2][i])
        axe.set_title(str(dests[i]) + " destination groups")
    #axes[0].set_title(" 2 destination groups")
    #axes[1].set_title(" 4 destination groups")
        #axes[0][i].set_title("Node " + str(nodes[i]))
        #axes[1][i].set_title("Node " + str(nodes[i]))
        for idx,row in data.iterrows():
            #if row['Protocol'] != "WbCast" and row['Config'] == 1: continue
            #if row['Clients'] not in [1,400,500,1000]: continue
            label_pos=[row['lat_avg'], row['msgps_avg']]
            if row['Clients'] not in [1,4000, 12000, 20000]: continue
            if row['Clients'] == 4000 and row['Protocol'] not in ['WbCast']: continue
            if row['Clients'] in [12000] and not ( row['Config'] >= 8 and row['Protocol'] in ['FastCast', 'Skeen']) : continue
            if row['Clients'] in [20000] and row['Protocol'] == 'Skeen' and row['Config'] <= 2: continue
            if row['Clients'] in [4000] and row['Protocol'] == 'Skeen' and row['Config'] >= 4: continue
            if row['Clients'] in [4000] and row['Protocol'] in ['Skeen','FastCast'] and row['Config'] == 1: continue
            if row['Clients'] > 4000:
                if row['Config'] == 1 and row['Protocol'] != "WbCast":
                    label_pos[1] *= 0.85
                if row['Config'] == 2 and row['Protocol'] != "WbCast":
                    label_pos[1] *= 1.15
                if row['Config'] == 4 and row['Protocol'] == "Skeen":
                    label_pos[1] *= 0.85
                if row['Config'] == 4 and row['Protocol'] == "FastCast":
                    label_pos[1] *= 0.75
                if row['Config'] == 8 and row['Protocol'] == "Skeen":
                    label_pos[1] *= 1.10
            axe.annotate(row['Clients'], tuple(label_pos))
        #axe.ticklabel_format(axis='y', style='sci', scilimits=(0,0))
    plt.tight_layout()
    plt.savefig('/mnt/graphs/' + fig_filename, bbox_inches='tight')
    #plt.savefig('/mnt/graphs/' + fig_filename)
    plt.close()
    return

"""
if __name__ == "__main__":
    cmp_stats = prepareCmpStats()
    dir_path="/mnt/dataframe/"
    #df_name="lan_10ch_mixed_dataframe.csv"
    df_name="wan_10ch_full_dataframe.csv"
    exported_df_path=dir_path+df_name
    cmp_stats = pd.read_table(exported_df_path, sep='\t', index_col=0)

    odir_path="/mnt/dataframe/rel_diff/"
    out_df_path=odir_path+"rel-diff_"+df_name
    cg_df_path=odir_path+"by-client_"+df_name
    dg_df_path=odir_path+"by-group_"+df_name
    ag_df_path=odir_path+"mean_"+df_name

    o_stats = pd.DataFrame()
    for name, group in cmp_stats.groupby(["Config","Clients"]):
        a_lat, a_msgps = group[group["Protocol"] == "amcast"][["lat_avg","msgps_avg"]].mean()
        f_lat, f_msgps = group[group["Protocol"] == "famcast"][["lat_avg","msgps_avg"]].mean()
        b_lat, b_msgps = group[group["Protocol"] == "basecast"][["lat_avg","msgps_avg"]].mean()
        stats = pd.DataFrame({"Config": name[0], "Clients": name[1],
                "rd_lat_fastcast": (abs(a_lat - f_lat) / min(a_lat,f_lat)),
                "rd_lat_basecast": (abs(a_lat - b_lat) / min(a_lat,b_lat)),
                "rd_msgps_fastcast": (abs(a_msgps - f_msgps) / min(a_msgps,f_msgps)),
                "rd_msgps_basecast": (abs(a_msgps - b_msgps) / min(a_msgps,b_msgps))}, index=range(1))
        o_stats = o_stats.append(stats, ignore_index=True)
    o_stats.to_csv(out_df_path, sep='\t')

    cg_stats = o_stats.groupby("Clients").mean().drop("Config", axis=1)
    cg_stats.to_csv(cg_df_path, sep='\t')

    dg_stats = o_stats.groupby("Config").mean().drop("Clients", axis=1)
    dg_stats.to_csv(dg_df_path, sep='\t')

    ag_stats = cg_stats.mean()
    ag_stats.to_csv(ag_df_path, sep='\t')
    for n_targets in cmp_stats["Config"].unique():
        for n_clients in cmp_stats[cmp_stats["Config"] == n_targets]["Clients"].unique():
            lat_avg_amcast = cmp_stats[(cmp_stats["Config"] == n_targets) & (cmp_stats["Clients"] == n_clients)]
            msgps_avg_amcast = cmp_stats[(cmp_stats["Config"] == n_targets) & (cmp_stats["Clients"] == n_clients)]
            stats = pd.DataFrame({"Destinations": n_targets, "Clients": n_clients,
                "rd_lat_fastcast": ,
                "rd_lat_basecast": ,
                "rd_msgps_fastcast": ,
                "rd_msgps_basecast": }, index=range(1))
    wan_stats = prepareCmpStats()
"""
"""
if __name__ == "__main__":
    cmp_stats = prepareCmpStats()
    ocmp_stats = prepareCmpStats()
    oocmp_stats = prepareCmpStats()

    exported_df_path="/mnt/dataframe/wan_10ch_noproxy_serialized_new2_dataframe.csv"
    #ocmp_stats = pd.read_csv(exported_df_path, sep='\t', index_col=0)
    #exported_df_path="/mnt/dataframe/lan_10ch_noproxy_serialized_5_dataframe.csv"
    cmp_stats = pd.read_csv(exported_df_path, sep='\t', index_col=0)
    exit()
    #cmp_stats = cmp_stats.append(ocmp_stats[ocmp_stats["Config"] == 2], ignore_index=True)
    #cmp_stats = cmp_stats.append(ocmp_stats[ocmp_stats["Config"] == 4], ignore_index=True)
    #cmp_stats = cmp_stats.append(ocmp_stats[ocmp_stats["Config"] == 6], ignore_index=True)
    #cmp_stats = cmp_stats.append(ocmp_stats[ocmp_stats["Config"] == 8], ignore_index=True)
    #cmp_stats = cmp_stats.append(ocmp_stats[ocmp_stats["Config"] == 1][ocmp_stats["Clients"] != 1], ignore_index=True)
    #cmp_stats = cmp_stats.append(ocmp_stats[ocmp_stats["Config"] == 10][ocmp_stats["Protocol"] != "amcast"], ignore_index=True)
    #cmp_stats = cmp_stats[cmp_stats["Clients"] <= 1500]

    cmp_stats["lat_avg"] = cmp_stats["lat_avg"] * 1000
    cmp_stats["msgps_avg"] = cmp_stats["msgps_avg"] / 1000
    cmp_stats.replace("amcast", "WbCast", inplace=True)
    cmp_stats.replace("basecast", "Skeen", inplace=True)
    cmp_stats.replace("famcast", "FastCast", inplace=True)

    plotCmpStatsLine(cmp_stats, "line.3R.10ch.wan.noproxy.serialized.new2.small.png")
"""
"""
if __name__ == "__main__":
    cmp_stats = prepareCmpStats()

    exported_df_path="/mnt/dataframe/wan_10ch_full_dataframe.csv"
    cmp_stats = pd.read_table(exported_df_path, sep='\t', index_col=0)

    cmp_stats["lat_avg"] = cmp_stats["lat_avg"] * 1000
    cmp_stats.replace("amcast", "WbCast", inplace=True)
    cmp_stats.replace("basecast", "Skeen", inplace=True)
    cmp_stats.replace("famcast", "FastCast", inplace=True)

    rows=[]
    rows += cmp_stats.index[(cmp_stats['Config'] == 10) & (cmp_stats['Clients'] == 1500)].tolist()
    rows += cmp_stats.index[(cmp_stats['Config'] == 10) & (cmp_stats['Clients'] == 5500)].tolist()
    rows += cmp_stats.index[(cmp_stats['Config'] == 10) & (cmp_stats['Clients'] == 6000)].tolist()
    rows += cmp_stats.index[(cmp_stats['Config'] == 4) & (cmp_stats['Clients'] == 7000)].tolist()
    rows += cmp_stats.index[(cmp_stats['Config'] == 2) & (cmp_stats['Clients'] == 14000)].tolist()
    rows += cmp_stats.index[(cmp_stats['Config'] == 1) & (cmp_stats['Clients'] == 10000)].tolist()
    rows += cmp_stats.index[(cmp_stats['Config'] == 1) & (cmp_stats['Clients'] == 20000)].tolist()
    rows += cmp_stats.index[(cmp_stats['Config'] == 1) & (cmp_stats['Clients'] == 18000)].tolist()
    cmp_stats.drop(cmp_stats.index[rows], inplace=True)

    plotCmpStatsLine(cmp_stats, "line.3R.10ch.wan.full.png")
"""
"""
if __name__ == "__main__":
    df_path="/mnt/dataframe/wan_10ch_noproxy_serialized_new2_dataframe.csv"
    cmp_stats = prepareCmpStats()
    cmp_stats = pd.read_csv(df_path, sep="\t", index_col=0)

    for proto in ["amcast", "famcast", "basecast"]:
        explogpath="/mnt/wan_10ch_noproxy_serialized_" + proto + "/"
        cmp_stats = computeCmpStats(explogpath, df_path, cmp_stats, proto)

    cmp_stats = pd.read_csv(df_path, sep="\t", index_col=0)
    cmp_stats.sort_values(by=["Protocol", "Config", "Clients"], inplace=True)
    cmp_stats.reset_index(drop=True, inplace=True)
    cmp_stats.to_csv(df_path, sep="\t")
"""
if __name__ == "__main__":
    explogpath="/mnt/wan_10ch_recovery_local_amcast/log.10.30.4.6000.10.24000000.1/"
    #explogpath="/mnt/wan_10ch_recovery_global_amcast/log.10.30.4.6000.10.24000000.1/"
    exp = Experiment(24000000)
    exp.importDir(explogpath)
    exp.recComputeStats()
    exp.recPlot("local_allg_recovery.png")
    #exp.recPlot("global_2_recovery.png")

    #explogpath="/mnt/wan_10ch_recovery_local_amcast/log.10.30.4.6000.10.24000000.1/"
    explogpath="/mnt/wan_10ch_recovery_global_amcast/log.10.30.4.6000.10.24000000.1/"
    exp = Experiment(24000000)
    exp.importDir(explogpath)
    exp.recComputeStats()
    #exp.recPlot("local_2_recovery.png")
    exp.recPlot("global_allg_recovery.png")
"""
    exported_df_path="/mnt/dataframe/lan_1ch_singlegroupcluster_dataframe.csv"
    cmp_stats.to_csv(exported_df_path, sep='\t')
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
