#define STRIGID_COMPONENT(Name, ...)                                    \
    struct Name {                                                       \
        struct Data { STRIGID_MEMBERS(__VA_ARGS__) };                   \
        using PtrTuple = std::tuple<STRIGID_PTRS(__VA_ARGS__)>;         \
        struct Shadow {                                                 \
            STRIGID_REFS(__VA_ARGS__)                                   \
            Shadow(STRIGID_CTOR_PARAMS(__VA_ARGS__))                    \
                : STRIGID_CTOR_INIT(__VA_ARGS__) {}                     \
        };                                                              \
    };

// Example expansion for (float, x, float, y):
// Data { float x; float y; };
// PtrTuple = std::tuple<float*, float*>;
// Shadow { float& x; float& y; Shadow(float& x_, float& y_) : x(x_), y(y_) {} };
