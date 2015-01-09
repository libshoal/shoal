#ifndef __SHL_INTERNAL_H
#define __SHL_INTERNAL_H

// for barrelfish


#include "shl.h"

struct shl__pci_address {
    uint32_t bus;       ///< the PCI bus the device is located on
    uint32_t device;    ///< device number
    uint32_t function;  ///< which function to use
    uint32_t count;     ///< number of functions this device has
};

/**
 * \brief setup information structure for initializing the memcpy facility
 */
struct shl__memcpy_setup {
    uint32_t count;             ///< the number of devices
    struct shl__pci_address *pci;
    struct {
        uint32_t vendor;    ///< the vendor of the device
        uint32_t id;        ///< the device id
    } device;
};

#ifdef PAPI
#include <papi.h>
#endif

// --------------------------------------------------
// Defines
#define SHL__ARRAY_NAME_LEN_MAX 100

// --------------------------------------------------
// Other things
void aff_set_oncpu(unsigned int cpu);

// --------------------------------------------------
// Query array configuration
void shl__lua_init(void);
void shl__lua_deinit(void);
bool shl__get_array_conf(const char* array_name, int feature, bool def);
int shl__get_global_conf(const char *table, const char *field, int def);


/*
 * -------------------------------------------------------------------------------
 * Memcopy initialization
 * -------------------------------------------------------------------------------
 */
#ifdef __cplusplus
extern "C" {
#endif

int shl__memcpy_init(struct shl__memcpy_setup *setup);

/* copying from and to an array  */
size_t shl__memcpy_dma_from(void *va_src, void *mi_dst, size_t size, size_t *tx_compl);
size_t shl__memcpy_dma_to(void *mi_src, void *va_dst, size_t size, size_t *tx_compl);
size_t shl__memcpy_dma_array(void *mi_src, void *mi_dst, size_t size, size_t *tx_compl);
size_t shl__memset_dma(void *mi_dst, uint64_t value, size_t size, size_t *tx_compl);


/* openmp memcpy/memset */
int shl__memcpy_openmp(void *dst, void *src, size_t element_size, size_t elements);
int shl__memset_openmp(void *dst, void *value, size_t element_size, size_t elements);

/* polling */
int shl__memcpy_poll(void);




#ifdef __cplusplus
}
#endif

#endif /* __SHL_INTERNAL_H */
