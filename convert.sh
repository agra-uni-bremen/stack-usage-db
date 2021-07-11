#!/bin/sh

[ $# -eq 2 ] || exit 1
fn=${2}

auxname=${fn%%.*}
printf "%s.su\n" "${auxname}"
