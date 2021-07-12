#!/bin/sh

[ $# -eq 1 ] || exit 1
fn=${1}

auxname=${fn%%.*}
printf "%s.su\n" "${auxname}"
