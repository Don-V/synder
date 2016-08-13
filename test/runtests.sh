#!/usr/bin/env bash
set -u

synder=$PWD/synder

total_passed=0
total_failed=0

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

# A function to select which parts of the output should be compared
# Since flags are currently in flux, test only the first 7 columns
filter () {
   sort
}
base_one_filter () {
    awk -v OFS="\t" '{print $1,$2,$3+1,$4+1,$5,$6+1,$7+1,$8,$9,$10,$11}' | sort
}

out_base=0
one_base_synder="$synder -b"

runtest(){
    dif=$1
    base=$2
    msg=$3
    errmsg=${4:-0}
    tmp=/tmp/synder-$RANDOM$RANDOM
    mkdir $tmp
    echo -n "Testing $msg ... "
    $synder -d $dir/map.syn a b $tmp/db 
    if [[ $? == 0 ]]
    then
        if [[ $out_base == 1 ]]
        then
            $synder -b -i $dir/$base.gff -s $tmp/db/a_b.txt -c search > $tmp/a
            diff <(cat $tmp/a | filter) \
                 <(cat $dir/${base}-exp.txt | base_one_filter) > /dev/null 
        else
            $synder -i $dir/$base.gff -s $tmp/db/a_b.txt -c search > $tmp/a
            diff <(cat $tmp/a | filter) \
                 <(cat $dir/${base}-exp.txt | filter) > /dev/null 
        fi

        if [[ $? == 0 ]]
        then
            total_passed=$(( $total_passed + 1 ))
            echo "OK"
        else
            echo
            warn "FAIL"
            echo " (in `basename $dir`/)"
            [[ $errmsg == 0 ]] || (echo -e $errmsg | fmt)
            total_failed=$(( $total_failed + 1 ))
            echo "======================================="
            emphasize_n "expected output"; echo ": (${base}-exp.txt)"
            cat $dir/${base}-exp.txt | filter | column -t
            emphasize "observed output:"
            cat $tmp/a | filter | column -t
            emphasize_n "query gff"; echo ": (${base}.gff)"
            column -t $dir/$base.gff
            emphasize_n "synteny map"; echo ": (map.syn)"
            column -t $dir/map.syn
            echo "See zzz_g and zzz_db"
            echo -e "---------------------------------------\n"

            ln -sf $dir/$base.gff zzz_g 
            $synder -d $dir/map.syn a b zzz_db

        fi

        rm -rf $tmp
    else
        total_failed=$(( $total_failed + 1 ))
        warn "FAILED - Could not build database\n"
    fi
}

#---------------------------------------------------------------------
dir="$PWD/test/test-data/one-block"
announce "\nTesting with synteny map length == 1"
runtest $dir hi     "query after of block"
runtest $dir within "query within block"
runtest $dir lo     "query before of block"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/two-block"
announce "\nTesting with synteny map length == 2"
runtest $dir hi      "query downstream of all blocks"
runtest $dir between "query between two blocks"
runtest $dir lo      "query upstream of all blocks"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/multi-block"
announce "\nTesting with 5 adjacent blocks on the same strand"
runtest $dir a "extreme left"
runtest $dir b "inbetween two adjacent blocks"
runtest $dir c "starts inbetween adjacent blocks"
runtest $dir d "stops inbetween adjacent blocks"
runtest $dir e "inbetween two adjacent blocks"
runtest $dir f "starts before block 3, ends after block 3"
runtest $dir g "starts in block 2, ends after block 3"
runtest $dir h "starts before block 2, ends after block 3"
runtest $dir i "starts in block 2, ends in block 2"
runtest $dir j "extreme right"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/simple-duplication"
announce "\nTest simple tandem duplication"
runtest $dir between "query starts between the duplicated intervals"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/one-interval-inversion"
announce "\nTest when a single interval is inverted"
runtest $dir between "query next to inverted interval"
runtest $dir over    "query overlaps inverted interval"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/two-interval-inversion"
announce "\nTest when two interval are inverted"
runtest $dir beside   "query next to inverted interval"
runtest $dir within   "query between inverted intervals"
runtest $dir spanning "query spans inverted intervals"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/tiny-indel-query-side"
announce "\nTest when a small interval interupts on one side"
runtest $dir beside "query side"
dir="$PWD/test/test-data/tiny-indel-target-side"
runtest $dir beside "target side"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/tandem-transposition"
announce "\nTest tandem transposition"
runtest $dir beside "query beside the transposed pair"
runtest $dir within "query between the transposed pair"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/irregular-overlaps"
announce "\nTest target side internal overlaps"
runtest $dir left "left side" "You are either 1) not sorting the by_stop vector
in Contig by Block stop positions, or 2) are snapping the search interval left
boundary to a Block that is nearest by start, but not be stop."
runtest $dir right "right side"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/multi-chromosome"
announce "\nTest two intervals on same query chr but different target chr"
runtest $dir between "between the query intervals"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/inverted-extremes"
announce "\nExtreme value resulting from an inversion"
runtest $dir extreme "between the query intervals, extreme SI"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/deletion"
announce "\nDeletion tests (adjacent bounds in target)"
runtest $dir between "query is inbetween"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/off-by-one"
announce "\nTest overlap edge cases"
runtest $dir a "overlap of 1"

#---------------------------------------------------------------------
dir="$PWD/test/test-data/unassembled"
announce "\nMappings beyond the edges of target scaffold"
runtest $dir lo "query is below scaffold"

out_base=1
runtest $dir lo "test with 1-base"

# # TODO Find a good way to deal with this case:
# dir="$PWD/test/test-data/synmap-overlaps"
# announce "\nsyntenic overlaps"
# runtest $dir simple "Between the weird"

#---------------------------------------------------------------------
echo

# valgrind tests
valgrind_checked=0
valgrind_exit_status=
test-valgrind () {
    if hash valgrind 2> /dev/null; then
        valgrind_checked=1
        make sample 2>&1 > /dev/null
        valgrind --leak-check=full $synder \
            -i g -s d -c search > /dev/null 2> valgrind.log
        valgrind_exit_status=$?
        make clean-sample 2>&1 > /dev/null
    fi
}


#=====================================================================
echo

total=$(( total_passed + total_failed))
emphasize "$total_passed tests successful out of $total"

test-valgrind
if [[ $valgrind_checked == 0 ]]
then
    warn "valgrind not found, no memory tests performed\n"
else
    if [[ $valgrind_exit_status == 0 ]]
    then
        emphasize_n "valgrind pure"
        echo " (for synder search of multi-block/c.gff against multi-block/map.syn)"
        rm valgrind.log
    else
        warn "valgrind failed - see valgrind.log\n"
    fi
fi

if [[ $total_failed > 0 ]]
then
    warn "$total_failed tests failed\n"
    exit 1
else
    exit 0
fi
