#!/bin/bash
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/data

compatible=$(cat /proc/device-tree/compatible)
chipname=""
case "$compatible" in
    *px30*)    chipname="px30" ;;
    *px3se*)  chipname="px3SE" ;;
    *rk1808*)  chipname="rk1808" ;;
    *rk3288*)  chipname="rk3288" ;;
    *rk3229gva*) chipname="rk3229GVA" ;;
    *rk3328*)  chipname="rk3328" ;;
    *rk3399pro*)
        chipname="rk3399pro";;
    *rk3399*)  chipname="rk3399" ;;
    *rk3326*)  chipname="rk3326" ;;
    *rk3328*)  chipname="rk3328" ;;
    *rk3358*)  chipname="rk3358" ;;
    *rk3128*)  chipname="rk3128" ;;
    *rk3506*)  chipname="rk3506" ;;
    *rk3528*)  chipname="rk3528" ;;
    *rk3562*)  chipname="rk3562" ;;
    *rk3566*)  chipname="rk3566" ;;
    *rk3568*)  chipname="rk3568" ;;
    *rk3572*)  chipname="rk3572" ;;
    *rk3576*)  chipname="rk3576" ;;
    *rk3588*)  chipname="rk3588" ;;
    *rk3036*)  chipname="rk3036" ;;
    *rk3308*)  chipname="rk3208" ;;
    *rv1126*)  chipname="rv1126" ;;
    *rv1109*)  chipname="rv1109" ;;
    *)
        echo "Please check if the SoC is supported on Rockchip Linux!"
        exit 1
        ;;
esac

echo "RK_CHIPNAME=$chipname" > /data/rk-chipname.conf