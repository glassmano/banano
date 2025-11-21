#!/bin/bash
set -eux

test_cmd() {
    netmatch="$1"
    netcmd="$2"
    netarg="$3"
    genesishash="$4"

    DATADIR=$(mktemp -d)

    # initialise data directory
    $NANO_NODE_EXE --initialize --data_path "$DATADIR" "$netcmd" "$netarg"

    # check that it is the live network
    grep -q "Active network: $netmatch" "$DATADIR"/log/log_*.log

    # check that the ledger file is created and has one block, the genesis block
    $NANO_NODE_EXE --debug_block_count --data_path "$DATADIR" "$netcmd" "$netarg" | grep -q 'Block count: 1'

    # check the genesis block is correct
    $NANO_NODE_EXE --debug_block_dump --data_path "$DATADIR" "$netcmd" "$netarg" | head -n 1 | grep -qi "$genesishash"
}

test_cmd "live" "--network" "live" "F61A79F286ABC5CC01D3D09686F0567812B889A5C63ADE0E82DD30F3B2D96463"
test_cmd "beta" "--network" "beta" "E1227CF974C1455A8B630433D94F3DDBF495EEAC9ADD2481A4A1D90A0D00F488"
test_cmd "test" "--network" "test" "B1D60C0B886B57401EF5A1DAA04340E53726AA6F4D706C085706F31BBD100CEE"

# if it got this far then it is a pass
exit 0
