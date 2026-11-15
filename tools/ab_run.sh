#!/bin/bash
# usage: tools/ab_run.sh <new.exe> <old.exe> <tag> [games=40] [ms=150] [seedBase=300]
# 5 parallel self A/B matches (1 thread each); prints combined W/D/L and Elo.
cd "$(dirname "$0")/.."
new=$1; old=$2; tag=$3; games=${4:-40}; ms=${5:-150}; seed=${6:-300}
rm -rf reports/$tag; mkdir -p reports/$tag
for i in 1 2 3 4 5; do mkdir -p reports/$tag/$i
 (cd reports/$tag/$i && VIRGIN_MATCH_SEED=$((seed+i)) nohup ../../../build/win/$new --match ../../../build/win/$old 2800 50 $games $ms 2800 v2800-ausgeglichen . 1 $ms > out.txt 2>&1 &)
done
sleep 30
while [ "$(tasklist | grep -ci "$new")" -gt 0 ]; do sleep 5; done
W=0;D=0;L=0
for i in 1 2 3 4 5; do
 W=$((W+$(grep -a '^  game' reports/$tag/$i/out.txt | grep -c WIN)))
 D=$((D+$(grep -a '^  game' reports/$tag/$i/out.txt | grep -c DRAW)))
 L=$((L+$(grep -a '^  game' reports/$tag/$i/out.txt | grep -c LOSS)))
done
echo "$new vs $old: +$W =$D -$L"
awk -v w=$W -v d=$D -v l=$L 'BEGIN{n=w+d+l;s=(w+d/2)/n;e=-400*log(1/s-1)/log(10);sd=sqrt((w*(1-s)^2+d*(0.5-s)^2+l*s^2)/n)/sqrt(n);printf "n=%d score=%.3f elo=%+.0f +/- %.0f\n",n,s,e,sd*1.96*400/log(10)/(s*(1-s))}'
