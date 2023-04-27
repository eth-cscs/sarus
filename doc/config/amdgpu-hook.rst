**************
AMD GPU hook
**************

The AMD GPU hook provides access to ROCm AMD GPU devices inside the container.

Hook installation
-----------------

The hook is written in C++ and is compiled when building Sarus without the need 
for additional dependencies. The Sarus installation scripts automatically install 
the hook in the ``$CMAKE_INSTALL_PREFIX/bin`` directory. 
Thus, there is no specific action required to install the AMD GPU hook.


Hook configuration
-------------------
The hook is designed to run as a **prestart** hook and does not accept any arguments. 
To enable the AMD GPU hook, use the following example of an OCI hook JSON configuration file:

.. literalinclude:: /config/hook_examples/11-amdgpu-hook.json
   :language: json

It is generally recommended to configure the hook with the ``"when": {"always": true}`` condition, 
as it can automatically determine whether it should take action.


Hook support at runtime
------------------------

The hook expects to find the following devices:

.. code-block:: bash

    /dev/kfd

    /dev/dri/card0
    /dev/dri/renderD128
    ...
    /dev/dri/cardN
    /dev/dri/renderD<128+N>

    /dev/by-path/pci-<bus>:00.0-card0
    /dev/by-path/pci-<bus>:00.0-render0
    ...
    /dev/by-path/pci-<bus>:00.0-cardN
    /dev/by-path/pci-<bus>:00.0-renderN

If the ``/dev/kfd`` device is not present, the hook is automatically deactivated.

The hook uses the environment variable `ROCR_VISIBLE_DEVICES`` to identify the GPU(s) 
that are made available within the container. If the variable is not present or empty, 
all GPU(s) are made available within the container. For more information about 
`ROCR_VISIBLE_DEVICES`, please refer to the following link: 
<https://rocmdocs.amd.com/en/latest/ROCm_System_Managment/ROCm-System-Managment.html#rocr-visible-devices>
