

#ifndef SHIM_H__INCLUDED
#define SHIM_H__INCLUDED

#include "common/rina_ids.h"

#ifdef __cplusplus
extern "C"
{
#endif

    struct shim_ops
    {
        // int (*init)(struct ipcp_factory_data *data);
        // int (*fini)(struct ipcp_factory_data *data);

        struct ipcpInstance *(*create)(ipcProcessId_t xIpcpId);
        // int (*destroy)(struct ipcp_factory_data *data,struct ipcp_instance *inst);
    };

#ifdef __cplusplus
}
#endif

#endif /* SHIM_H__*/
