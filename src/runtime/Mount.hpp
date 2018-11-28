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
