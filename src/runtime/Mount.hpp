/*
 * Sarus
 *
 * Copyright (c) 2018-2019, ETH Zurich. All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef sarus_runtime_Mount_hpp
#define sarus_runtime_Mount_hpp

namespace sarus {
namespace runtime {

class Mount {
public:
    virtual void performMount() const = 0;
};

}
}

#endif
