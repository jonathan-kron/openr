#!/usr/bin/env python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.


import json
from typing import Any, Callable, Dict, Optional, Set, Tuple

import bunch

import click
from openr.clients.openr_client import get_openr_ctrl_client
from openr.KvStore import ttypes as kv_store_types
from openr.thrift.KvStore import types as kv_store_types_py3
from openr.utils import printing
from openr.utils.consts import Consts


class OpenrCtrlCmd:
    """
    Command wrapping OpenrCtrl.Client
    """

    def __init__(self, cli_opts: Optional[bunch.Bunch] = None) -> None:
        """initialize the Config Store client"""
        if not cli_opts:
            cli_opts = bunch.Bunch()

        self.cli_opts: bunch.Bunch = cli_opts
        self.host = cli_opts.get("host") or Consts.DEFAULT_HOST
        self.timeout = cli_opts.get("timeout") or Consts.DEFAULT_TIMEOUT
        self.fib_agent_port = (
            cli_opts.get("fib_agent_port") or Consts.DEFAULT_FIB_AGENT_PORT
        )
        self._config = None

    def run(self, *args, **kwargs) -> int:
        """
        run method that invokes _run with client and arguments
        """

        ret_val: Optional[int] = 0
        with get_openr_ctrl_client(self.host, self.cli_opts) as client:
            ret_val = self._run(client, *args, **kwargs)
            if ret_val is None:
                ret_val = 0
        return ret_val

    def _run(self, client: Any, *args, **kwargs) -> Any:
        """
        To be implemented by sub-command.
        @param: client - Client to connect to the Open/R server.
                         Set it to `Any` type here for the overridden method to choose the type in its parameter.
                         Currently, we have two types of the clients:
                                1. OpenrCtrl.Client for common APIs;
                                2. OpenrCtrlCpp client which implements stream APIs.
        """

        raise NotImplementedError

    def _get_config(self) -> Dict[str, Any]:
        if self._config is None:
            with get_openr_ctrl_client(self.host, self.cli_opts) as client:
                resp = client.getRunningConfig()
                self._config = json.loads(resp)
        return self._config

    def iter_dbs(
        self,
        container: Any,
        dbs: Dict,
        nodes: set,
        parse_func: Callable[[Any, Dict], None],
    ):
        """
        parse prefix/adj databases

        @param: container - container to store the generated data and returns
        @param: dbs - decision_types.PrefixDbs or decision_types.AdjDbs
        @param: nodes - set: the set of nodes for parsing
        @param: parse_func - function: the parsing function
        """
        for (node, db) in sorted(dbs.items()):
            if "all" not in nodes and node not in nodes:
                continue
            parse_func(container, db)

    def print_stats(self, stats_templates, counters):
        """
        Print in pretty format
        """

        suffixes = [".60", ".600", ".3600", ""]

        for template in stats_templates:
            counters_rows = []
            for title, key in template["counters"]:
                val = counters.get(key, None)
                counters_rows.append([title, "N/A" if not val and val != 0 else val])

            stats_cols = ["Stat", "1 min", "10 mins", "1 hour", "All Time"]
            stats_rows = []
            for title, key_prefix in template["stats"]:
                row = [title]
                for key in ["{}{}".format(key_prefix, s) for s in suffixes]:
                    val = counters.get(key, None)
                    row.append("N/A" if not val and val != 0 else val)
                stats_rows.append(row)

            if "title" in template:
                print("\n> {} ".format(template["title"]))

            if counters_rows:
                print()
                print(
                    printing.render_horizontal_table(
                        counters_rows, tablefmt="plain"
                    ).strip("\n")
                )
            if stats_rows:
                print()
                print(
                    printing.render_horizontal_table(
                        stats_rows, column_labels=stats_cols, tablefmt="simple"
                    ).strip("\n")
                )

    # common function used by decision, kvstore mnodule
    def buildKvStoreKeyDumpParams(
        self,
        prefix: str = Consts.ALL_DB_MARKER,
        originator_ids: Optional[Set[str]] = None,
        keyval_hash: Optional[Dict[str, kv_store_types.Value]] = None,
    ) -> kv_store_types.KeyDumpParams:
        """
        Build KeyDumpParams based on input parameter list
        """
        params = kv_store_types.KeyDumpParams(prefix)
        params.originatorIds = originator_ids if originator_ids else None
        params.keyValHashes = keyval_hash if keyval_hash else None
        if prefix:
            params.keys = [prefix]

        return params

    def fetch_initialization_events(
        self, client: Any
    ) -> Dict[kv_store_types.InitializationEvent, int]:
        """
        Fetch Initialization events as a dictionary via thrift call
        """

        return client.getInitializationEvents()

    def validate_init_event(
        self,
        init_event_dict: Dict[kv_store_types.InitializationEvent, int],
        init_event: kv_store_types.InitializationEvent,
    ) -> Tuple[bool, Optional[str], Optional[str]]:
        """
        Returns True if the init_event specified is published within it's defined time limit
        If init_event is published, returns the duration as a stylized string. If the checks fail,
        returns False along with an error message.
        """

        # Keeps track of whether or not the checks pass
        is_pass = True

        # If the check fails, this will hold the error msg string
        err_msg_str = None

        # If the init_event is published, stores the duration as a stylized string
        dur_str = None

        # TODO: (klu02) Update the client to thrift.py3 so we can use thrift.py3 types instead of legacy
        init_event_name = kv_store_types.InitializationEvent._VALUES_TO_NAMES[
            init_event
        ]

        if init_event not in init_event_dict:
            is_pass = False
            err_msg_str = f"{init_event_name} event is not published"
        else:
            warning_label = kv_store_types_py3.InitializationEventTimeLabels[
                f"{init_event_name}_WARNING_MS"
            ]
            timeout_label = kv_store_types_py3.InitializationEventTimeLabels[
                f"{init_event_name}_TIMEOUT_MS"
            ]

            warning_time = kv_store_types_py3.InitializationEventTimeDuration[
                warning_label
            ]
            timeout_time = kv_store_types_py3.InitializationEventTimeDuration[
                timeout_label
            ]

            init_event_dur = init_event_dict[init_event]

            if init_event_dur < warning_time:
                dur_str = click.style(str(init_event_dur), fg="green")
            elif init_event_dur < timeout_time:
                dur_str = click.style(str(init_event_dur), fg="yellow")
            else:
                dur_str = click.style(str(init_event_dur), fg="red")
                err_msg_str = f"{init_event_name} event duration exceeds acceptable time limit (>{timeout_time}ms)"
                is_pass = False

        return is_pass, err_msg_str, dur_str
