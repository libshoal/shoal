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


static uint8_t dma_device_count = 0;
struct dma_device **dma_devices;
static uint8_t dma_device_next = 0;

///< how much data is transferred per channel
#define DMA_CHANNEL_STRIDE (1024 * 1024)

// DMA devices
#define PCI_DEVICE_IOAT_HSW0    0x2f20
#define PCI_DEVICE_IOAT_HSW_CNT 8
#define PCI_DEVICE_IOAT_IVB0    0x0e20
#define PCI_DEVICE_IOAT_IVB_CNT 8

/*
 * -------------------------------------------------------------------------------
 * DMA initialization functions
 * -------------------------------------------------------------------------------
 */

#ifndef __k1om__
static void ioat_device_init(struct device_mem *bar_info,
                             int nr_mapped_bars)
{
    errval_t err;

    if (nr_mapped_bars != 1) {
        return;
    }

    /* initialize the device */
    err = ioat_dma_device_init(*bar_info->frame_cap,
                               (struct ioat_dma_device**)&dma_devices[dma_device_count]);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Could not initialize the device.\n");
        return;
    }

    dma_device_count++;
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
    if (dma_devices == NULL) {
        return -1;
    }

    for (uint32_t i = 0; i < setup->count; ++i) {
        err = pci_register_driver_noirq(ioat_device_init, PCI_DONT_CARE,
                                        PCI_DONT_CARE, PCI_DONT_CARE,
                                        setup->device.vendor,setup->device.id + i,
                                        setup->pci.bus, setup->pci.device, i);
        if (err_is_fail(err)) {
            if (dma_device_count == 0) {
                DEBUG_ERR(err, "initializing the device");
                return -3;
            }
        }
    }

    SHL_DEBUG_MEMCPY("IOAT DMA device initialized... %u\n", dma_device_count);

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

#if 0

/*
 * -------------------------------------------------------------------------------
 * DMA copy
 * -------------------------------------------------------------------------------
 */






int shl__memcpy_dma(void *va_src, void *mi_dst, size_t size)
{
    unsigned long paddr = (unsigned long)shl__get_physical_address(va_src);

    struct shl_mi_header *mihdr = mi_dst;

    unsigned long *dst = calloc(mihdr->num, sizeof(unsigned long));
    for (int j = 0; j < mihdr->num; j++) {
        dst[j] = mihdr->data[j].paddr;
    }




    size_t done_counter = 0;
    size_t counter = 0;
    size_t offset = 0;


    while(bytes > 0) {
        if (bytes < STRIDE_DIVIDER * sizeof(T)) {
            for (int j = 0; j < num_replicas; j++) {
                for (unsigned int i = offset; i < shl_array<T>::size; i++) {
                    rep_array[j][i] = src[i];
                }
            }
            bytes = 0;
            break;
        } else {
            counter++;
            for (int j = 0; j < num_replicas; j++) {
                shl__memcpy_dma_async((unsigned long)paddr, dst[j], STRIDE_DIVIDER * sizeof(T), &done_counter);
                counter++;
            }
            offset += STRIDE_DIVIDER;
            bytes -= STRIDE_DIVIDER * sizeof(T);
            paddr += STRIDE_DIVIDER * sizeof(T);
        }
    }

    while(done_counter != counter) {
        shl__memcpy_dma_poll();
    }
}

int shl__memcpy_dma_array(void *mi_src, void *mi_dst, size_t size)
{
    return 0;
}

int shl__memcpy_dma_async(lpaddr_t src, lpaddr_t dst, size_t bytes, void *arg)
{

}

int shl__memcpy_dma(lpaddr_t src, lpaddr_t dst, size_t bytes)
{
    errval_t err;

    size_t transfer_done = 0;
    size_t transfer_count = (bytes / DMA_CHANNEL_STRIDE) + 1;

    for (size_t i = 0; i < transfer_count; ++i) {
        shl__memcpy_dma_async(src, dst, bytes, &transfer_done);
    }


    while(transfer_done != transfer_count) {
        struct dma_device *dev = dma_devices[dma_device_next];

        if (dma_device_next < dma_device_count) {
            dma_device_next++;
        } else {
            dma_device_next = 0;
        }

        err = dma_device_poll_channels(dev);
        if (err_is_fail(err)) {

        }
    }

    return 0;
}

#endif

static errval_t shl__get_physical_address(lvaddr_t vaddr,
                                          lpaddr_t *retaddr, size_t *retsize)
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


#define DMA_TRANSFER_STRIDE (1024UL*1024)

#define DMA_MINTRANSFER 64


static void memcpy_req_cb(errval_t err, dma_req_id_t id, void *st)
{
    size_t *tx_done = st;
    (*tx_done) = (*tx_done) + 1;
}

static int do_dma_cpy(lpaddr_t to, lpaddr_t from, size_t bytes, void *counter)
{
    errval_t err;

#if 0
    SHL_DEBUG_MEMCPY("executing DMA: 0x%016" PRIxLPADDR " -> 0x%016" PRIxLPADDR
                     " of size %" PRIu64 " bytes\n", from, to, bytes);
#endif
    /* must be a multiple of chache lines */
    assert(!(bytes & (64 -1 )));

    if (dma_devices == NULL) {
        return -1;
    }

    if (dma_device_next == dma_device_count) {
        dma_device_next = 0;
    }

    struct dma_device *dev = dma_devices[dma_device_next];
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

    dma_device_next++;

    err = dma_request_memcpy(dev, &setup, NULL);
    if (err_is_fail(err)) {
        return -1;
    }


    return 0;
}

static int do_dma_poll(void)
{
    errval_t err;

    if (dma_device_next == dma_device_count) {
        dma_device_next = 0;
    }

    struct dma_device *dev = dma_devices[dma_device_next];

    err = dma_device_poll_channels(dev);
    if (err_is_fail(err)) {
        SHL_DEBUG_MEMCPY("polling device failed: %s\n", err_getstring(err));
        return -1;
    }

    dma_device_next++;

    return 0;
}

size_t shl__memcpy_dma_from(void *va_src, void *mi_dst, size_t offset, size_t size)
{
    errval_t err;

    lpaddr_t paddr = 0;

    size_t framesize = 0;

    err = shl__get_physical_address((lvaddr_t)va_src, &paddr, &framesize);
    if (err_is_fail(err)) {
        SHL_DEBUG_MEMCPY("preparing dma transfer failed: %s\n", err_getstring(err));
        return 0; // return 0 bytes
    }

    struct shl_mi_header *mihdr = mi_dst;

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

            do_dma_cpy(dma_to, dma_from, mihdr->stride, &transfer_done);
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

            do_dma_cpy(dma_to, dma_from, (size-copied), &transfer_done);
            transfer_count++;
        }


    } else {
        /* also do strided way */
        size_t copied = 0;
        lpaddr_t dma_from = paddr + offset;
        size_t iterations = size / DMA_TRANSFER_STRIDE;
        for (size_t k = 0; k < iterations; ++k) {
            for (int i = 0; i < mihdr->num; ++i) {
                lpaddr_t dma_to = mihdr->data[i].paddr + offset + copied;
                do_dma_cpy(dma_to, dma_from, DMA_TRANSFER_STRIDE, &transfer_done);
                transfer_count++;
            }
            dma_from += DMA_TRANSFER_STRIDE;
            copied += DMA_TRANSFER_STRIDE;
        }

        if (copied < size) {
            /* copy remainder */
            for (int i = 0; i < mihdr->num; ++i) {
                lpaddr_t dma_to = mihdr->data[i].paddr + offset + copied;
                do_dma_cpy(dma_to, dma_from, size - copied, &transfer_done);
                transfer_count++;
            }
        }
    }

    /* copy remainder */

    if (prependcpy) {
        SHL_DEBUG_MEMCPY("copying unaligned start: %" PRIu64 " bytes\n", prependcpy);
        if (mihdr->vaddr) {
            void *to = (void *)(mihdr->vaddr + offset);
            void *src = va_src + offset;
            memcpy(to, src, prependcpy);
        } else {
            /* copy beginning */
            for (int i = 0; i < mihdr->num; ++i) {
                void *to = (void *)(mihdr->data[i].vaddr + offset);
                void *src = va_src + offset;
                memcpy(to, src, prependcpy);
            }
        }
    }

    if (remainder) {
        SHL_DEBUG_MEMCPY("copying remainder: %" PRIu64 " bytes\n", remainder);
        if (mihdr->vaddr) {
            void *to = (void *)(mihdr->vaddr + offset + size);
            void *src = va_src + (offset + size);
            memcpy(to, src, remainder);
        } else {
            for (int i = 0; i < mihdr->num; ++i) {
                void *to = (void *)(mihdr->data[i].vaddr + offset + size);
                void *src = va_src + (offset + size);
                memcpy(to, src, remainder);
            }
        }
    }

    SHL_DEBUG_MEMCPY("waiting for DMA to complete...\n");
    size_t current_done = transfer_done;
    while(transfer_done < transfer_count) {
        if (current_done != transfer_done) {
            SHL_DEBUG_MEMCPY("waiting for DMA to complete... %" PRIu64 " of %"
                             PRIu64 "\n", transfer_done, transfer_count);
            current_done = transfer_done;
        }
        do_dma_poll();
    }

    SHL_DEBUG_MEMCPY("DMA done.\n");

    return size;
}


size_t shl__memcpy_dma_to(void *mi_src, void *va_dst, size_t offst, size_t size)
{
    return 0;
}

/* copying between arrays */
size_t shl__memcpy_dma_array(void *mi_src, void *mi_dst, size_t offst, size_t size)
{
    return 0;
}

size_t shl__memcpy_dma_async(void *mi_src, void *mi_dst, size_t offst, size_t size, uint32_t *done)
{
    return 0;
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
        return 0;
    }

    debug_printf("shl__memcpy_init\n");

    switch (setup->device.vendor) {
        case PCI_VENDOR_INTEL :
            if (((uint16_t)setup->device.id & 0xFFF0) == PCI_DEVICE_IOAT_IVB0
                 || ((uint16_t)setup->device.id & 0xFFF0) == PCI_DEVICE_IOAT_HSW0){
                return ioat_init(setup);
            } else if (setup->device.id == 0x0000) {
                /* TODO:
                 *
                 */
                return xeon_phi_init(setup);
            }
            break;
        case 0x1234 :
            return client_init(setup);
        default :
            return -1;
            break;
    }

    /* invalid device type */
    return -1;
}




void** shl__copy_array(void *src,
                       size_t size,
                       bool is_used,
                       bool is_ro,
                       const char* array_name)
{


    assert(!"NYI");
    return 0;
}

void shl__copy_back_array(void **src,
                          void *dest,
                          size_t size,
                          bool is_copied,
                          bool is_ro,
                          bool is_dynamic,
                          const char* array_name)
{
    assert(!"NYI");
}

void shl__copy_back_array_single(void *src,
                                 void *dest,
                                 size_t size,
                                 bool is_copied,
                                 bool is_ro,
                                 bool is_dynamic,
                                 const char* array_name)
{
    assert(!"NYI");
}
