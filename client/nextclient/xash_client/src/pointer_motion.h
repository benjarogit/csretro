#pragma once

namespace csr::client
{
    struct PointerHooks
    {
        void (*absolute)(void*, int*, int*);
        int (*relative_mode)(void*);
        void (*relative_delta)(void*, int*, int*);
        int (*visible)(void*);
        void (*move)(void*, int, int);
        void (*frame)(void*);
        void* owner;
    };

    class PointerMotion
    {
    public:
        // Synchronize the real position before VGUI advances its frame. Raw
        // deltas are consumed only while the UI owns a visible cursor; the
        // original game retains them throughout ordinary mouse-look.
        void Update(int wide, int tall, const PointerHooks& hooks);
        void Reset() { relative_ = false; }
    private:
        int x_{};
        int y_{};
        bool relative_{};
    };
}
