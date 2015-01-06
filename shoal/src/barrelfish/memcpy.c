/*
 * Copyright (c) 2014 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <barrelfish/barrelfish.h>

#include <pci/pci.h>

#include "shl_internal.h"
#include <backend/barrelfish/meminfo.h>

/* generic DMA */
#include <dma/dma.h>
#include <dma/dma_device.h>
#include <dma/dma_request.h>

/* for initializing IOAT DMA device */
#include <dma/ioat/ioat_dma.h>
#include <dma/ioat/ioat_dma_device.h>

/* for Xeon Phi Devices */
#include <dma/xeon_phi/xeon_phi_dma.h>
#include <dma/xeon_phi/xeon_phi_dma_device.h>

/* for DMA Service Clients */
#include <dma/client/dma_client.h>
#include <dma/client/dma_client_device.h>

static uint8_t dma_node_count = 0;
struct dma_device ***dma_devices = NULL;
static uint8_t *dma_device_next = NULL;
static uint8_t *dma_device_count = NULL;


///< how much data is transferred per channel
#define DMA_CHANNEL_STRIDE (1024 * 1024)
#define DMA_NODE_DONT_CARE 0xff
// DMA devices
#define PCI_DEVICE_IOAT_HSW0    0x2f20
#define PCI_DEVICE_IOAT_HSW_CNT 8
#define PCI_DEVICE_IOAT_IVB0    0x0e20
#define PCI_DEVICE_IOAT_IVB_CNT 8


#define MIN(a,b) ((a) < (b) ? (a) : (b))

/*
 * -------------------------------------------------------------------------------
 * DMA initialization functions
 * -------------------------------------------------------------------------------
 */

#ifndef __k1om__
static void ioat_device_init(struct device_mem *bar_info, int nr_mapped_bars)
{
    errval_t err;

    if (nr_mapped_bars != 1) {
        return;
    }

    struct ioat_dma_device **ioat_dev = (struct ioat_dma_device**)dma_devices[dma_node_count];

    /* initialize the device */
    err = ioat_dma_device_init(*bar_info->frame_cap, &ioat_dev[dma_device_count[dma_node_count]]);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Could not initialize the device.\n");
        return;
    }

    dma_device_count[dma_node_count]++;
}

static int ioat_init(struct shl__memcpy_setup *setup)
{
    errval_t err;

    SHL_DEBUG_MEMCPY("initializing IOAT DMA device...\n");

    err = pci_client_connect();
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not connect to the PCI domain\n");
        return -2;
    }

    dma_devices = calloc(setup->count, sizeof(void *));
    dma_device_count = calloc(2 * setup->count, sizeof(uint8_t));
    if (dma_devices == NULL || dma_device_count == NULL) {
        return -1;
    }

    dma_device_next = dma_device_count + setup->count;

    for (int i = 0; i < setup->count; ++i) {
        dma_devices[i] = calloc(setup->pci->count, sizeof(void *));
    }

    uint32_t dbg_device_count = 0;

    for (int node = 0; node < setup->count; ++node) {
        for (int dev = 0; dev < setup->pci[node].count; ++dev) {
            err = pci_register_driver_noirq(ioat_device_init, PCI_DONT_CARE,
                                            PCI_DONT_CARE, PCI_DONT_CARE,
                                            setup->device.vendor,
                                            setup->device.id + dev,
                                            setup->pci[node].bus,
                                            setup->pci[node].device, dev);
            if (err_is_fail(err)) {
                if (dma_device_count[dma_node_count] == 0) {
                    DEBUG_ERR(err, "initializing the device skipping node");
                    break;
                }
            }
            dbg_device_count++;
        }

        if (err_is_fail(err)) {
            if (dma_node_count == 0 && dma_device_count[0] == 0) {
                DEBUG_ERR(err, "initializing the device no nodes");
                return -3;
            }
        }

        if (dma_device_count[0]) {
            dma_node_count++;
        }
    }

    for (int i = 0; i < dma_node_count; ++i) {
        debug_printf("DMA[%u]: %u devices\n", i, dma_device_count[i]);
    }

    SHL_DEBUG_MEMCPY("IOAT DMA device initialized... %u\n", dbg_device_count);

    return 0;
}
#else
/* there is no IOAT DMA available on the Xeon Phi */
static int ioat_init(struct shl__memcpy_setup *setup)
{
    return -2;
}
#endif
static int xeon_phi_init(struct shl__memcpy_setup *setup)
{
    // setting default values for now to make compiler happy
    dma_device_count = 0;
    dma_devices = NULL;
    dma_device_next = 0;
    return -2;
}

static int client_init(struct shl__memcpy_setup *setup)
{
    return -2;
}

static errval_t shl__get_physical_address(lvaddr_t vaddr, lpaddr_t *retaddr,
                                          size_t *retsize)
{
    errval_t err;

    struct capref frame;
    struct frame_identity fi;

    struct pmap *p = get_current_pmap();
    genvaddr_t offset;
    size_t size;

    err = p->f.lookup(p, vaddr, NULL, &size, &frame, &offset, NULL);
    if (err_is_fail(err)) {
        return err;
    }

    err = invoke_frame_identify(frame, &fi);
    if (err_is_fail(err)) {
        return err;
    }

    if (offset != 0) {
        printf("!WWARNIGN! OFFSET IS NOT 0\n");
    }

    if (retaddr) {
        *retaddr = fi.base;
    }

    if (retsize) {
        *retsize = size;
    }

    return SYS_ERR_OK;
}

#define DMA_TRANSFER_STRIDE (2UL * 1024*1024)

#define DMA_MINTRANSFER 64

static void memcpy_req_cb(errval_t err, dma_req_id_t id, void *st)
{
    size_t *tx_done = st;
    (*tx_done) = (*tx_done) + 1;
}

static int do_dma_cpy(uint8_t dma_node, lpaddr_t to, lpaddr_t from, size_t bytes, void *counter)
{
    errval_t err;

    SHL_DEBUG_MEMCPY("executing DMA: 0x%016" PRIxLPADDR " -> 0x%016" PRIxLPADDR
                    " of size %" PRIu64 " bytes\n", from, to, bytes);
    /* must be a multiple of chache lines */
    assert(!(bytes & (64 - 1)));

    if (dma_devices == NULL) {
        return -1;
    }

    if (dma_node_count <= dma_node) {
        dma_node = 0;
    }

    if (dma_device_next[dma_node] == dma_device_count[dma_node]) {
        dma_device_next[dma_node] = 0;
    }


    struct dma_device **devices = dma_devices[dma_node];
    struct dma_device *dev = devices[dma_device_next[dma_node]];
    assert(dev);

    struct dma_req_setup setup = {
        .type = DMA_REQ_TYPE_MEMCPY,
        .done_cb = memcpy_req_cb,
        .cb_arg = counter,
        .args = {
            .memcpy = {
                .src = from,
                .dst = to,
                .bytes = bytes
            }
        }
    };

    dma_device_next[dma_node]++;

    err = dma_request_memcpy(dev, &setup, NULL);
    if (err_is_fail(err)) {
        return -1;
    }

    return 0;
}

static int do_dma_set(uint8_t dma_node, lpaddr_t to, uint64_t value, size_t bytes, void *counter)
{
    errval_t err;

    SHL_DEBUG_MEMCPY("executing DMA: 0x%016" PRIxLPADDR " -> 0x%016" PRIxLPADDR
                    " of size %" PRIu64 " bytes\n", from, to, bytes);

    /* must be a multiple of chache lines */
    assert(!(bytes & (64 - 1)));

    if (dma_devices == NULL) {
        return -1;
    }

    if (dma_node_count <= dma_node) {
        dma_node = 0;
    }

    if (dma_device_next[dma_node] == dma_device_count[dma_node]) {
        dma_device_next[dma_node] = 0;
    }

    struct dma_device **devices = dma_devices[dma_node];
    struct dma_device *dev = devices[dma_device_next[dma_node]];
    assert(dev);

    struct dma_req_setup setup = {
        .type = DMA_REQ_TYPE_MEMSET,
        .done_cb = memcpy_req_cb,
        .cb_arg = counter,
        .args = {
            .memset = {
                .val = value,
                .dst = to,
                .bytes = bytes
            }
        }
    };

    dma_device_next[dma_node]++;

    err = dma_request_memset(dev, &setup, NULL);
    if (err_is_fail(err)) {
        return -1;
    }

    return 0;
}

static int do_dma_poll(void)
{
    errval_t err;

    for (int node = 0; node < dma_node_count; ++node) {
        struct dma_device **devices = dma_devices[node];
        for (int i = 0; i < dma_device_count[node]; ++i) {
            struct dma_device *dev = devices[i];
            err = dma_device_poll_channels(dev);
            switch (err_no(err)) {
                case SYS_ERR_OK:
                case DMA_ERR_DEVICE_IDLE:
                    continue;
                    break;
                default:
                    /* no-op */
                    break;
            }
    }
    }

    return 0;
}

size_t shl__memset_dma(void *mi_dst, uint64_t value, size_t size)
{

    size_t remainder = (size & (DMA_MINTRANSFER - 1));
    if (remainder) {
        SHL_DEBUG_MEMCPY("remainder cpy: %" PRIu64 " bytes\n", remainder);
        size -= remainder;
    }

    struct shl_mi_header *mihdr = mi_dst;

    size_t transfer_count = mihdr->num;
    size_t transfer_completed = 0;

    for (int i = 0; i < mihdr->num; ++i) {
        lpaddr_t dst = mihdr->data[i].paddr;

        size_t bytes = size;
        if (mihdr->stride) {
            bytes = mihdr->stride;
        }

        while (bytes > (2 * DMA_TRANSFER_STRIDE)) {
            if (do_dma_set(i, dst, value, DMA_TRANSFER_STRIDE, &transfer_completed)) {
                //return 0;
            }
            transfer_count++;
            dst += DMA_TRANSFER_STRIDE;
            bytes -= DMA_TRANSFER_STRIDE;
        }
        if (do_dma_set(i, dst, value, bytes, &transfer_completed)) {
            //return 0;
        }
    }

    if (remainder) {
        SHL_DEBUG_MEMCPY("memset remainder: %" PRIu64 " bytes\n", remainder);

        uint64_t *to;
        if (mihdr->vaddr) {
            to = (uint64_t *) (mihdr->vaddr + size);
            for (int i = 0; i < remainder / sizeof(uint64_t); ++i) {
                to[i] = value;
            }
        } else {
            for (int i = 0; i < mihdr->num; ++i) {
                to = (uint64_t *) (mihdr->data[i].vaddr + size);
                for (int j = 0; j < remainder / sizeof(uint64_t); ++j) {
                    to[j] = value;
                }
            }
        }
    }

    do {
        do_dma_poll();
    } while (transfer_completed < transfer_count);

    SHL_DEBUG_MEMCPY("DMA done.\n");

    return size;
}

static size_t shl__memcpy_dma_phys(uint8_t node, lpaddr_t dst, lpaddr_t src, size_t bytes,
                                   size_t *counter)
{
    size_t transfer_count = 1;
    uint8_t target_node = 0;
    if (node != DMA_NODE_DONT_CARE) {
        target_node = MIN(node, dma_node_count);
    }

    //debug_printf("shl__memcpy_dma_phys: %lu bytes\n", bytes);

    if (node == DMA_NODE_DONT_CARE) {
        while (bytes > (2 * DMA_TRANSFER_STRIDE)) {\
            do_dma_cpy(target_node, dst, src, DMA_TRANSFER_STRIDE, counter);
            transfer_count++;
            target_node++;
            dst += DMA_TRANSFER_STRIDE;
            src += DMA_TRANSFER_STRIDE;
            bytes -= DMA_TRANSFER_STRIDE;
            if (target_node == dma_node_count) {
                target_node = 0;
            }
        }
    } else {
        while (bytes > (2 * DMA_TRANSFER_STRIDE)) {
            do_dma_cpy(target_node, dst, src, DMA_TRANSFER_STRIDE, counter);
            transfer_count++;
            dst += DMA_TRANSFER_STRIDE;
            src += DMA_TRANSFER_STRIDE;
            bytes -= DMA_TRANSFER_STRIDE;
        }

    }

    do_dma_cpy(target_node, dst, src, bytes, counter);

    return transfer_count;
}

size_t shl__memcpy_dma_from(void *va_src, void *mi_dst, size_t offset, size_t size)
{
    errval_t err;

    struct shl_mi_header *mihdr = mi_dst;

    /*
     * heuristical check if it makes sense to do a DMA copy.
     * There is a certain overhead in DMA transfers. We have to have at
     * least a minimum trasnfers size of SHL_DMA_COPY_THRESHOLD
     */
    if (((mihdr->stride != 0) && (mihdr->stride < SHL_DMA_COPY_THRESHOLD)) || size
                    < SHL_DMA_COPY_THRESHOLD) {
        debug_printf("shl__memcpy_dma_from error: not copying in...\n");
        return 0;
    }

    lpaddr_t paddr = 0;

    size_t framesize = 0;

    err = shl__get_physical_address((lvaddr_t) va_src, &paddr, &framesize);
    if (err_is_fail(err)) {
        return 0;  // return 0 bytes
    }

    SHL_DEBUG_MEMCPY("preparing dma transfer: 0x%016" PRIxLPADDR " of size %"
                    PRIu64 " bytes\n", paddr, size);

    assert(framesize < offset + size);

    size_t prependcpy = ((paddr + offset) & (DMA_MINTRANSFER - 1));
    if (prependcpy) {
        prependcpy = DMA_MINTRANSFER - prependcpy;
        SHL_DEBUG_MEMCPY("offset prependcopy: %" PRIu64 " bytes\n", prependcpy);
        size -= prependcpy;
        offset += prependcpy;
    }

    size_t remainder = (size & (DMA_MINTRANSFER - 1));
    if (remainder) {
        SHL_DEBUG_MEMCPY("remainder cpy: %" PRIu64 " bytes\n", remainder);
        size -= remainder;
    }

    assert(prependcpy < DMA_MINTRANSFER);
    assert(remainder < DMA_MINTRANSFER);

    size_t transfer_done = 0;
    size_t transfer_count = 0;

    void *counter = (&transfer_done);

    if (mihdr->stride) {
        /* this means we have a partitioned or distributed array */
        /* stride is a multiple of page size */
        assert(!(mihdr->stride & (PAGESIZE -1)));

        lpaddr_t dma_from = paddr + offset;
        int current = 0;
        size_t copied = 0;

        size_t iterations = size / mihdr->stride;

        size_t stride_offset = offset;

        for (size_t k = 0; k < iterations; ++k) {
            if (current == mihdr->num) {
                current = 0;
                stride_offset += mihdr->stride;
            }

            lpaddr_t dma_to = mihdr->data[current].paddr + stride_offset;

            do_dma_cpy(current, dma_to, dma_from, mihdr->stride, counter);
            transfer_count++;

            dma_from += mihdr->stride;
            copied += mihdr->stride;
            current++;
        }

        if (copied < size) {
            if (current == mihdr->num) {
                current = 0;
                stride_offset += mihdr->stride;
            }

            lpaddr_t dma_to = mihdr->data[current].paddr + stride_offset;

            do_dma_cpy(current, dma_to, dma_from, (size - copied), counter);
            transfer_count++;
        }

    } else {
        /* also do replicated/single way */
        size_t copied = 0;
        lpaddr_t dma_from = paddr + offset;
        size_t iterations = size / DMA_TRANSFER_STRIDE;
        for (size_t k = 0; k < iterations; ++k) {
            for (int i = 0; i < mihdr->num; ++i) {
                lpaddr_t dma_to = mihdr->data[i].paddr + offset + copied;
                do_dma_cpy(i, dma_to, dma_from, DMA_TRANSFER_STRIDE, counter);
                transfer_count++;
            }
            dma_from += DMA_TRANSFER_STRIDE;
            copied += DMA_TRANSFER_STRIDE;
        }

        if (copied < size) {
            /* copy remainder */
            for (int i = 0; i < mihdr->num; ++i) {
                lpaddr_t dma_to = mihdr->data[i].paddr + offset + copied;
                do_dma_cpy(i, dma_to, dma_from, size - copied, counter);
                transfer_count++;
            }
        }
    }

    /* copy remainder */

    if (prependcpy) {
        SHL_DEBUG_MEMCPY("copying unaligned start: %" PRIu64 " bytes\n", prependcpy);
        if (mihdr->vaddr) {
            void *to = (void *) (mihdr->vaddr + offset);
            void *src = va_src + offset;
            memcpy(to, src, prependcpy);
        } else {
            /* copy beginning */
            for (int i = 0; i < mihdr->num; ++i) {
                void *to = (void *) (mihdr->data[i].vaddr + offset);
                void *src = va_src + offset;
                memcpy(to, src, prependcpy);
            }
        }
    }

    if (remainder) {
        SHL_DEBUG_MEMCPY("copying remainder: %" PRIu64 " bytes\n", remainder);
        if (mihdr->vaddr) {
            void *to = (void *) (mihdr->vaddr + offset + size);
            void *src = va_src + (offset + size);
            memcpy(to, src, remainder);
        } else {
            for (int i = 0; i < mihdr->num; ++i) {
                void *to = (void *) (mihdr->data[i].vaddr + offset + size);
                void *src = va_src + (offset + size);
                memcpy(to, src, remainder);
            }
        }
    }

    do {
        do_dma_poll();
    } while (transfer_done < transfer_count);

    SHL_DEBUG_MEMCPY("DMA done.\n");

    return size;
}

size_t shl__memcpy_dma_to(void *mi_src, void *va_dst, size_t offset, size_t size)
{
    return 0;
}

/* copying between arrays */
size_t shl__memcpy_dma_array(void *mi_src, void *mi_dst, size_t size)
{
    /* check if the DMA transfer is big enough */
    if (size < SHL_DMA_COPY_THRESHOLD) {
        return 0;
    }

    struct shl_mi_header *mi_hdr_dst = mi_dst;
    struct shl_mi_header *mi_hdr_src = mi_src;

    SHL_DEBUG_MEMCPY("shl__memcpy_dma_array: %lx, %lx, %lu, %lu\n", mi_hdr_dst->stride,
                    mi_hdr_src->stride, mi_hdr_dst->num, mi_hdr_src->num);

    /*
     * doing DMA transfers is actually possible between Arrays of the same type
     * if their memory configuration matches (i.e. stride size for distributed arrays)
     */

    if (mi_hdr_dst->stride != mi_hdr_src->stride) {
        /* the strides do not match, we have different types of arrays */

        if (mi_hdr_dst->stride > 0 && mi_hdr_src->stride > 0) {
            return 0;
        }

        if (mi_hdr_dst->stride > 0 && mi_hdr_dst->stride < SHL_DMA_COPY_THRESHOLD) {
            /* destination array is a distributed array, not supported */
            debug_printf("destination is distribution. not supported");
            return 0;
        }

        if (mi_hdr_src->stride > 0 && mi_hdr_src->stride < SHL_DMA_COPY_THRESHOLD) {
            /* source array is a distributed array, not supported */
            debug_printf("source is distribution. not supported");
            return 0;
        }
    }

    // assuming that the start is aligned for now
    // and strides are aligned
    size_t remainder = (size & (DMA_MINTRANSFER - 1));
    if (remainder) {
        SHL_DEBUG_MEMCPY("remainder cpy: %" PRIu64 " bytes\n", remainder);
        size -= remainder;
    }

    assert(remainder < DMA_MINTRANSFER);

    /*
     * at this point the arrays are of the same type or one of the supported
     * inter-types transfers:
     * SN<->REP, PART<->SN, PART<->REP
     */

    size_t transfers_completed = 0;
    size_t transfer_count = 0;

    if (mi_hdr_dst->stride) {
        /* destination array is partitioned or distributed */


        if (mi_hdr_src->stride) {
            assert(mi_hdr_dst->stride == mi_hdr_src->stride);

            /*
             * src array is partitioned or distributed and hence both arrays
             * share the same underlying data structure. Simpy copy the frames
             */
            assert(mi_hdr_dst->stride == mi_hdr_src->stride);
            assert(mi_hdr_dst->num == mi_hdr_src->num);

            /* we can simply copy the contents of all the frames */
            for (int i = 0; i < mi_hdr_dst->num; ++i) {
                /*
                 * issue the transfers. Note that the frames are all well aligned
                 * and the stride is already a multiple of the page size
                 */
                transfer_count += shl__memcpy_dma_phys(i, mi_hdr_dst->data[i].paddr,
                                                       mi_hdr_src->data[i].paddr,
                                                       mi_hdr_dst->stride,
                                                       &transfers_completed);
            }
        } else {
            assert(mi_hdr_dst->stride > SHL_DMA_COPY_THRESHOLD);

            /* stride should be size/num nodes i.e. a partitioned array */
            assert(mi_hdr_dst->stride >= (size / mi_hdr_dst->num));

            lpaddr_t src = mi_hdr_src->data[0].paddr;

            for (int i = 0; i < mi_hdr_dst->num; ++i) {
                transfer_count += shl__memcpy_dma_phys(i, mi_hdr_dst->data[i].paddr,
                                                       src, mi_hdr_dst->stride,
                                                       &transfers_completed);
                src += mi_hdr_dst->stride;
            }
        }

    } else {
        /* destination array is single node or replicated */

        if (mi_hdr_src->stride) {
            /* src array is partitioned */
            assert(mi_hdr_src->stride >= (size / mi_hdr_src->num));

            size_t transfer_size = mi_hdr_src->stride;
            lpaddr_t offset = 0;
            for (int j = 0; j < mi_hdr_src->num; ++j) {
                lpaddr_t src = mi_hdr_src->data[j].paddr;
                for (int i = 0; i < mi_hdr_dst->num; ++i) {
                    transfer_count += shl__memcpy_dma_phys(DMA_NODE_DONT_CARE,
                                    mi_hdr_dst->data[i].paddr + offset, src,
                                    transfer_size, &transfers_completed);
                }
                offset += transfer_size;
            }
        } else {
            /* source array is single node or replicated */
            for (int i = 0; i < mi_hdr_dst->num; ++i) {
                for (int j = 0; j < mi_hdr_src->num; ++j) {
                    transfer_count += shl__memcpy_dma_phys(DMA_NODE_DONT_CARE,
                                                           mi_hdr_dst->data[i].paddr,
                                                           mi_hdr_src->data[j].paddr,
                                                           size,
                                                           &transfers_completed);
                }
            }
        }
    }

    if (remainder) {
        SHL_DEBUG_MEMCPY("copying remainder: %" PRIu64 " bytes\n", remainder);

        void *va_src;
        if (mi_hdr_src->vaddr) {
            va_src = (void *) (mi_hdr_src->vaddr + size);
        } else {
            va_src = (void*) (mi_hdr_src->data[0].vaddr + size);
        }

        if (mi_hdr_dst->vaddr) {
            void *to = (void *) (mi_hdr_dst->vaddr + size);
            memcpy(to, va_src, remainder);
        } else {
            for (int i = 0; i < mi_hdr_dst->num; ++i) {
                void *to = (void *) (mi_hdr_dst->data[i].vaddr + size);
                memcpy(to, va_src, remainder);
            }
        }
    }

    do {
        do_dma_poll();
    } while (transfers_completed < transfer_count)

    SHL_DEBUG_MEMCPY    ("DMA done.\n");

    return size;
}

size_t shl__memcpy_dma_async(void *mi_src, void *mi_dst, size_t size, uint32_t *done)
{
    return 0;
}

static inline int shl__memcpy_openmp1(uint8_t *dst, uint8_t *src, size_t elements)
{
//#pragma omp parallel for
    for (size_t i = 0; i < elements; ++i) {
        dst[i] = src[i];
    }
    return elements;
}
static inline int shl__memcpy_openmp2(uint16_t *dst, uint16_t *src, size_t elements)
{
//#pragma omp parallel for
    for (size_t i = 0; i < elements; ++i) {
        dst[i] = src[i];
    }
    return elements;
}
static inline int shl__memcpy_openmp4(uint32_t *dst, uint32_t *src, size_t elements)
{
//#pragma omp parallel for
    for (size_t i = 0; i < elements; ++i) {
        dst[i] = src[i];
    }
    return elements;
}
static inline int shl__memcpy_openmp8(uint64_t *dst, uint64_t *src, size_t elements)
{
//#pragma omp parallel for
    for (size_t i = 0; i < elements; ++i) {
        dst[i] = src[i];
    }
    return elements;
}

int shl__memcpy_openmp(void *dst, void *src, size_t element_size, size_t elements)
{
    switch(element_size) {
        case 1:
            return shl__memcpy_openmp1(dst, src, elements);
        case 2:
            return shl__memcpy_openmp2(dst, src, elements);
        case 4:
            return shl__memcpy_openmp4(dst, src, elements);
        case 8:
            return shl__memcpy_openmp8(dst, src, elements);
        default:
            memcpy(dst, src, element_size * elements);
            return 0;
    }
}



static inline int shl__memset_openmp1(uint8_t *dst, uint8_t *value, size_t elements)
{
 //   #pragma omp parallel
    {
        uint8_t val = *value;
  //      #pragma omp parallel for
        for (size_t i = 0; i < elements; ++i) {
            dst[i] = val;
        }
    }
    return elements;
}
static inline int shl__memset_openmp2(uint16_t *dst, uint16_t *value, size_t elements)
{
  //  #pragma omp parallel
    {
        uint32_t val = *value;
   //     #pragma omp parallel for
        for (size_t i = 0; i < elements; ++i) {
            dst[i] = val;
        }
    }
    return elements;
}
static inline int shl__memset_openmp4(uint32_t *dst, uint32_t *value, size_t elements)
{
 //   #pragma omp parallel
    {
        uint32_t val = *value;
   //     #pragma omp parallel for
        for (size_t i = 0; i < elements; ++i) {
            dst[i] = val;
        }
    }
    return elements;
}
static inline int shl__memset_openmp8(uint64_t *dst, uint64_t *value, size_t elements)
{
 //   #pragma omp parallel
    {
        uint64_t val = *value;
   //     #pragma omp parallel for
        for (size_t i = 0; i < elements; ++i) {
            dst[i] = val;
        }
    }
    return elements;
}

int shl__memset_openmp(void *dst, void *value, size_t element_size, size_t elements)
{
    switch(element_size) {
        case 1:
            return shl__memset_openmp1(dst, value, elements);
        case 2:
            return shl__memset_openmp2(dst, value, elements);
        case 4:
            return shl__memset_openmp4(dst, value, elements);
        case 8:
            return shl__memset_openmp8(dst, value, elements);
        default:
            assert(!"wrong size");
            return 0;
    }
}



/**
 * \brief initializes the copy infrastructure
 *
 * \returns 0 on SUCCESS
 *          non zero on FAILURE
 */
int shl__memcpy_init(struct shl__memcpy_setup *setup)
{
    // we use memcpy()
    if (setup == NULL) {
        return -1;
    }

    debug_printf("shl__memcpy_init\n");

    switch (setup->device.vendor) {
        case PCI_VENDOR_INTEL:
            if (((uint16_t) setup->device.id & 0xFFF0) == PCI_DEVICE_IOAT_IVB0
                            || ((uint16_t) setup->device.id & 0xFFF0)  == PCI_DEVICE_IOAT_HSW0) {
                return ioat_init(setup);
            } else if (setup->device.id == 0x0000) {
                /* TODO:
                 *
                 */
                return xeon_phi_init(setup);
            }
            break;
        case 0x1234:
            return client_init(setup);
        default:
            return -1;
            break;
    }

    /* invalid device type */
    return -1;
}

void** shl__copy_array(void *src, size_t size, bool is_used, bool is_ro,
                       const char* array_name)
{

    assert(!"NYI");
    return 0;
}

void shl__copy_back_array(void **src, void *dest, size_t size, bool is_copied,
                          bool is_ro, bool is_dynamic, const char* array_name)
{
    assert(!"NYI");
}

void shl__copy_back_array_single(void *src, void *dest, size_t size, bool is_copied,
                                 bool is_ro, bool is_dynamic, const char* array_name)
{
    assert(!"NYI");
}
