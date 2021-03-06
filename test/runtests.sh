#!/usr/bin/env bash
set -u

announce(){
    [[ -t 1 ]] && o="\e[1;33m$1\e[0m" || o=$1
    echo -e $o
}

warn(){
    [[ -t 1 ]] && o="\e[1;31m$1\e[0m" || o=$1
    echo -en $o
}

emphasize(){
   emphasize_n "$1" 
   echo
}

emphasize_n(){
    [[ -t 1 ]] && o="\e[1;39m$1\e[0m" || o=$1
    echo -ne $o
}

usage (){
cat << EOF
Test the final output of synder
OPTIONAL ARGUMENTS
  -h  print this help message
  -x  die on first failure 
  -m  test memory with valgrind
  -d  print full debugging info
  -v  print verbose debugging info
  -o  redirect gdb output to this file (or tty)
  -a  archive all results
EOF
    exit 0
}

die_on_fail=0
valgrind=0
debug=0
verbose=0
gdb_out="none"
archive=0
while getopts "hdxvma:o:" opt; do
    case $opt in
        h)
            usage ;;
        x)
            die_on_fail=1 ;;
        m)
            if [[ ! -z `type -P valgrind` ]]
            then
                valgrind=1
            else
                warn "Valgrind not found"   
            fi
            ;;
        d) 
            debug=1 ;;
        v)
            verbose=1 ;;
        o)
            gdb_out=$OPTARG
            ;;
        a)
            archive=$OPTARG
            mkdir -p $archive
            ;;
        ?)
            echo "Illegal arguments"
            exit 1
    esac 
done

synder=$PWD/synder

total_passed=0
total_failed=0
valgrind_checked=0
valgrind_exit_status=1
synder_exit_status=1
diff_exit_status=1


# A function to select which parts of the output should be compared
# Since flags are currently in flux, test only the first 7 columns
filter () {
    sort | cut -f1-8,11-13
}

filter_plus_one () {
    awk -v OFS="\t" '{$3++ ; $4++ ; $6++ ; $7++ ; print}' | filter
}

set_defaults () {
    g_dir=
    g_arg=
    g_exp_ext=
    g_map=map.syn
}

# This variable will be set for each set of test
# It specifies where the input files can be found
set_defaults

g_test_num=0
runtest(){
    base=$1
    msg=$2
    errmsg=${3:-0}
    out_base=${4:-0}

    synder_cmd=

    # temp file for special error messages that need to be sent to user
    tmpmsg=/tmp/m

    >$tmpmsg

    echo -n "Testing $msg ... "

    fail=0

    [[ -z $g_exp_ext ]] && g_exp_ext=exp

    # initialize temporary files
    gff=input.gff
    map=synteny-map.tab
    gdbcmd=.gdb_cmd
    obs=observed-output
    exp=expected-output
    val=valgrind-log
    log=error-log
    run=run
    gdb=gdb
    syn=synmap.txt

    tempfiles="$gff $map $gdbcmd $obs $exp $val $log $gdb $run $syn"

    cp $g_dir/$base.gff $gff
    cp $g_dir/$g_map    $map

    if [[ $out_base == 1 ]]
    then
        cat $g_dir/${base}-${g_exp_ext}.txt | filter_plus_one > $exp
    else
        cat $g_dir/${base}-${g_exp_ext}.txt | filter > $exp
    fi

    synder_cmd=$synder

    [[ $out_base == 1 ]] && synder_cmd="$synder_cmd -b 0011 "
    synder_cmd="$synder_cmd -i $gff"
    synder_cmd="$synder_cmd -s $map"
    synder_cmd="$synder_cmd -c search"
    synder_cmd="$synder_cmd $g_arg"

    # Query genome length file
    tgen=$g_dir/tgen.tab
    # Target genome length file
    qgen=$g_dir/qgen.tab

    # If the length files are given, add to db command
    if [[ -f $tgen ]]
    then
        synder_cmd="$synder_cmd -t $tgen"
    fi

    if [[ -f $qgen ]]
    then
        synder_cmd="$synder_cmd -q $qgen"
    fi

    # command for loading into gdb
    echo "set args $synder_cmd"  >  $gdbcmd
    # this must go before sourcing .cmds.gdb for breakpoints to work
    echo "file $synder"          >> $gdbcmd
    echo "source $PWD/.cmds.gdb" >> $gdbcmd
    if [[ $gdb_out != "none" ]]
    then
        echo "set logging off"                   >> $gdbcmd
        echo "set logging file $gdb_out"         >> $gdbcmd
        echo "set logging redirect on"           >> $gdbcmd
        echo "set logging on"                    >> $gdbcmd
        echo "gdb -tui --command $gdbcmd -d $PWD" > $gdb
        chmod 755 $gdb
    fi

    # Ensure all input files are readable
    for f in $gff $exp $map;
    do
        if [[ ! -r "$f" ]]
        then
            warn "input:"
            fail=1
        fi
    done

    $synder_cmd > $obs 2> $log
    synder_exit_status=$?

    if [[ $valgrind -eq 1 ]]
    then
        # append valgrind messages to any synder error messages
        valgrind --leak-check=full $synder_cmd > $obs 2> $val
        grep "ERROR SUMMARY: 0 errors" $val > /dev/null
        valgrind_exit_status=$?
        if [[ $valgrind_exit_status -ne 0 ]]
        then
            warn "valgrind:"
            fail=1
        fi
    fi

    $synder_cmd -D > /dev/null 2> $syn
    if [[ $? -ne 0 ]]
    then
        if [[ $valgrind_exit_status -eq 0 || $synder_exit_status -eq 0 ]]
        then
            warn "dump:"
            synder_exit_status=1
            fail=1
        fi
    fi

    if [[ $synder_exit_status != 0 ]]
    then
        warn "runtime:"
        fail=1
    fi

    filter < $obs > /tmp/z && mv /tmp/z $obs
    diff $exp $obs 2>&1 > /dev/null
    diff_exit_status=$?

    if [[ $fail -eq 0 && $diff_exit_status -ne 0 ]]
    then
        warn "logic:"
        fail=1
    fi

    if [[ $fail -eq 0 ]]
    then
        echo "OK"
        total_passed=$(( $total_passed + 1 ))
    else
        warn "FAIL\n"
        total_failed=$(( $total_failed + 1 ))
    fi

    # Print expected and observed output, for successful failures
    if [[ $fail -ne 0 && $diff_exit_status -ne 0 ]]
    then
        [[ $errmsg == 0 ]] || (echo -e $errmsg | fmt)
        echo "======================================="
        emphasize_n "test directory"; echo ": `basename $g_dir`"
        emphasize_n "expected output"; echo ": (${base}-exp.txt)"
        cat $exp
        emphasize "observed output:"
        cat $obs
        emphasize_n "query gff"; echo ": (${base}.gff)"
        column -t $gff
        emphasize_n "synteny map"; echo ": (map.syn)"
        column -t $map
        if [[ -s $tmpmsg ]]
        then
            emphasize "messages:"
            cat $tmpmsg
        fi
        if [[ $debug -eq 1 && $verbose -eq 1 ]]
        then
            echo "Debugging files:"
            echo " * g - input GFF file"
            echo " * o - observed output"
            echo " * e - expected output"
            echo " * d - database directory"
            echo " * v - valgrind log (empty on success)"
            echo " * l - error log (synder STDERR output)"
            echo " * s - synmap dump"
            echo " * c - gdb command"
            echo " * x - initialize gdb"
            echo " * r - run the command that failed"
            echo "Synder command:"
            echo $synder_cmd
        fi
        echo -e "---------------------------------------\n"
    fi

    if [[ -d $archive ]]
    then
        g_test_num=$(( g_test_num + 1 ))
        if [[ $fail -ne 0 ]]
        then
            state="F"
        else
            state="P"
        fi
        arch="$archive/${state}_${g_test_num}_`basename $g_dir`"
        [[ -d $arch ]] && rm -rf $arch
        mkdir -p $arch
        mv $tempfiles $arch 2> /dev/null
        echo $synder_cmd > $arch/$run
        chmod 755 $arch/$run
    fi

    # clear all temporary files
    rm -rf $tempfiles

    # Reset all values
    gff= map= cmd= obs= exp= val= log=

    [[ $fail -ne 0 && $die_on_fail -ne 0 ]] && exit 1
}

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/one-block"
announce "\nTesting with synteny map length == 1"
runtest hi     "query after of block"
runtest within "query within block"
runtest lo     "query before of block"

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/two-block"
announce "\nTesting with synteny map length == 2"
runtest hi      "query downstream of all blocks"
runtest between "query between two blocks"
runtest lo      "query upstream of all blocks"

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/multi-block"
announce "\nTesting with 5 adjacent blocks on the same strand"
runtest a "extreme left"
runtest b "inbetween two adjacent blocks"
runtest c "starts inbetween adjacent blocks"
runtest d "stops inbetween adjacent blocks"
runtest e "inbetween two adjacent blocks"
runtest f "starts before block 3, ends after block 3"
runtest g "starts in block 2, ends after block 3"
runtest h "starts before block 2, ends after block 3"
runtest i "starts in block 2, ends in block 2"
runtest j "extreme right"

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/simple-duplication"
announce "\nTest simple tandem duplication"
runtest between "query starts between the duplicated intervals"

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/one-interval-inversion"
announce "\nTest when a single interval is inverted"
runtest between "query next to inverted interval"
runtest over    "query overlaps inverted interval"

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/two-interval-inversion"
announce "\nTest when two interval are inverted"
runtest beside   "query next to inverted interval"
runtest within   "query between inverted intervals"
runtest spanning "query spans inverted intervals"

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/tandem-transposition"
announce "\nTest tandem transposition"
runtest beside "query beside the transposed pair"
runtest within "query between the transposed pair"

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/irregular-overlaps"
announce "\nTest target side internal overlaps"
runtest left "left side" "You are either 1) not sorting the by_stop vector
in Contig by Block stop positions, or 2) are snapping the search interval left
boundary to a Block that is nearest by start, but not be stop."
runtest right "right side"

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/off-by-one"
announce "\nTest overlap edge cases"
runtest a "overlap of 1"

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/inverted-extremes"
announce "\nExtreme value resulting from an inversion"
runtest extreme "between the query intervals, extreme SI"

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/deletion"
announce "\nDeletion tests (adjacent bounds in target)"
runtest between "query is inbetween"

# ---------------------------------------------------------------------
g_dir="$PWD/test/test-data/unassembled"
announce "\nMappings beyond the edges of target scaffold"
runtest lo "query is below scaffold"
runtest adj-lo "query is just below the scaffold"
runtest adj-hi "query is just above the scaffold"
runtest hi "query is above the scaffold"
runtest lo "test with 1-base" 0 1

# ---------------------------------------------------------------------
announce "\nTest multi-chromosome cases when k=0"
g_arg=" -k 0 "
#  T   =====[---->
#        |
#  Q   =====   <->   =====
#                      |
#  T           <----]=====
g_dir="$PWD/test/test-data/interruptions/multi-chromosome"
runtest between "interuption between query intervals"
#  T   =====[-------------]=====
#        |                   |
#  Q   ===== <->   =====   =====
#                    |
#  T        ===[--]=====
#            |
#  Q        ===
g_dir="$PWD/test/test-data/interruptions/one-query-side"
runtest beside "query side"
# T             ===    ===
#                |      |
# Q    =====[--]===    ===[--]=====
#        |                      |
# T    =====   <-->           =====
g_dir="$PWD/test/test-data/interruptions/two-target-side"
runtest beside "target side"
g_arg=" -k 1 "
runtest beside "target side, k=1 (should be the same)"
# T    =====                      =====
#        |                          |
# Q    =====   ===== <--> =====   =====
#                |          |
# T            =====[----]=====
g_dir="$PWD/test/test-data/interruptions/two-query-side"
ark=" -k 0 "
runtest between "between two interior query-side intervals (k=0)"

# ---------------------------------------------------------------------
set_defaults
g_arg=' -k 4 '
announce "\nConfirm two-scaffold systems are unaffected by k"
g_dir="$PWD/test/test-data/tandem-transposition"
runtest beside "query beside the transposed pair"
runtest within "query between the transposed pair"
g_dir="$PWD/test/test-data/simple-duplication"
runtest between "query starts between the duplicated intervals"

# ---------------------------------------------------------------------
set_defaults
announce "\nTest multi-chromosome cases when k=2"
g_arg=" -k 2 "
g_exp_ext='exp-k2'
#  T   =====[-------------]=====
#        |                   |
#  Q   ===== <->   =====   =====
#                    |
#  T        ===[--]=====
#            |
#  Q        ===
g_dir="$PWD/test/test-data/interruptions/one-query-side"
runtest beside "query side"
#           [----------------]
# T             ===    ===
#                |      |
# Q    =====    ===    ===    =====
#        |                      |
# T    =====   <-->           =====
g_dir="$PWD/test/test-data/interruptions/two-target-side"
runtest beside "target side"
# T    =====[--------------------]=====
#        |                          |
# Q    =====   ===== <--> =====   =====
#                |          |
# T            =====[----]=====
g_dir="$PWD/test/test-data/interruptions/two-query-side"
runtest between "between two interior query-side intervals (k=2)"
# T    =====[------------------------------------]=====
#        |                                          |
# Q    =====   =====   ===== <--> =====   =====   =====
#                |       |          |       |
# T            =====[----|----------|----]=====
#                        |          |
#                      =====[----]=====
g_dir="$PWD/test/test-data/interruptions/nested"
g_arg=" -k 4 "
g_exp_ext="exp-k4"
runtest between "query nested two pairs of interrupting intervals (k=4)"
g_arg=" -k 3 "
g_exp_ext="exp-k3"
runtest between "query nested two pairs of interrupting intervals (k=3)"

# ---------------------------------------------------------------------
set_defaults
g_dir="$PWD/test/test-data/synmap-overlaps"
announce "\nsyntenic overlaps"
runtest simple "Between the weird"

# g_dir="$PWD/test/test-data/build/big"
# build-test "$g_dir/c.syn" "$g_dir/c.gff" "Stress test"

# ---------------------------------------------------------------------
set_defaults
announce "\ndouble overlapping tests"
g_dir="$PWD/test/test-data/build/overlap-tests"
g_map="map-1.syn"
runtest a "Overlap - single nesting"
g_map="map-2.syn"
runtest a "Overlap - triple identical"
g_map="map-3.syn"
runtest a "Overlap - left"
g_map="map-4.syn"
runtest a "Overlap - left-right"
g_map="map-5.syn"
runtest a "Overlap - double nest"
g_map="map-6.syn"
runtest a "Overlap - double nest left"
g_map="map-7.syn"
runtest a "Overlap - Q-inside T-right"
g_map="map-8.syn"
runtest a "Overlap - Tangles"
g_map="map-9.syn"
runtest a "Overlap - double overlap on different target contigs"

# ---------------------------------------------------------------------
echo

total=$(( total_passed + total_failed))
emphasize "$total_passed tests successful out of $total"

if [[ $total_failed > 0 ]]
then
    warn "$total_failed tests failed\n"
    exit 1
else
    exit 0
fi
