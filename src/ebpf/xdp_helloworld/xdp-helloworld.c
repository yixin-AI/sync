// file: xdp-helloworld.c

#include <linux/bpf.h>

#ifndef __section
# define __section(NAME)                  \
   __attribute__((section(NAME), used))
#endif



__section("prog")
int xdp_drop(struct xdp_md *ctx)
{
    static int example_count = 1;

    example_count++;

    if (example_count%2)
    {
        return XDP_DROP;
    }
    else
    {
        return XDP_PASS;
    }
}

char __license[] __section("license") = "GPL";


