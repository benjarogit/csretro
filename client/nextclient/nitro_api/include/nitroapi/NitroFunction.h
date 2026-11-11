#pragma once

#include <type_traits>

#include "chain/HandlersChainInterface.h"
#include "ChainExtensions.h"
#include "Concepts.h"

namespace nitroapi
{
    template<class TResult, class... TArgs>
    class NitroFunctionBase
    {
    protected:
        HandlersChainInterface<TResult, TArgs...>* chain_ = nullptr;
        GenericHookInterface<TResult, TArgs...>* hook_ = nullptr;

    public:
        void SetChain(HandlersChainInterface<TResult, TArgs...>* chain)
        {
            if (chain_ != nullptr)
                chain_->DetachHook();

            chain_ = chain;
        }

        void SetHook(GenericHookInterface<TResult, TArgs...>* hook)
        {
            if (chain_ != nullptr)
                chain_->DetachHook();

            hook_ = hook;

            AttachHookIfNeeded();
        }

        bool AttachHook()
        {
            if (chain_ == nullptr || hook_ == nullptr)
                return false;

            chain_->AttachHook(hook_);
            return true;
        }

        bool DetachHook()
        {
            if (chain_ == nullptr)
                return false;

            chain_->DetachHook();
            return true;
        }

        HandlersChainInterface<TResult, TArgs...>* GetChain() const
        {
            return this->chain_;
        }

        GenericHookInterface<TResult, TArgs...>* GetHook() const
        {
            return this->hook_;
        }

    protected:
        // No hook object means the address was never resolved. A hook object that
        // reports address 0 can still be a test double whose Invoke runs the
        // callback, so only a null hook is skipped here.
        bool HasHook() const
        {
            return hook_ != nullptr;
        }

        static TResult MissingTarget()
        {
            if constexpr (std::is_void_v<TResult>)
                return;
            else
                return TResult{};
        }

        void AttachHookIfNeeded()
        {
            if (hook_ != nullptr && chain_->GetHook() == nullptr && (chain_->GetSubscribersCount() > 0 || chain_->GetHandlersCount() > 0))
                chain_->AttachHook(hook_);
        }
    };

    // ------------------------------------------------------------------------------------------

    template<class TResult, class... TArgs>
    class NitroFunction : public NitroFunctionBase<TResult, TArgs...>
    {
    public:
        TResult operator()(TArgs... args) const
        {
            if (!this->HasHook())
                return this->MissingTarget();

            return this->hook_->InvokeOriginal(std::forward<TArgs>(args)...);
        }

        TResult InvokeChained(TArgs... args) const
        {
            if (!this->HasHook())
                return this->MissingTarget();

            return this->hook_->Invoke(std::forward<TArgs>(args)...);
        }

        std::shared_ptr<Unsubscriber> operator|=(const std::function<TResult(TArgs..., NextHandlerInterface<TResult, TArgs...>*)>& func)
        {
            auto unsubscriber = (*this->chain_) |= func;

            NitroFunctionBase<TResult, TArgs...>::AttachHookIfNeeded();

            return unsubscriber;
        }

        std::shared_ptr<Unsubscriber> operator+=(const std::function<void(TArgs..., TResult)>& func)
        {
            auto unsubscriber = (*this->chain_) += func;

            NitroFunctionBase<TResult, TArgs...>::AttachHookIfNeeded();

            return unsubscriber;
        }
    };

    template<FunctionPointer TFunc, class TResult, class... TArgs>
    class NitroFunctionTyped : public NitroFunction<TResult, TArgs...>
    {
    public:
        inline TFunc GetFunc() const
        {
            if (!this->HasHook())
                return nullptr;

            if (this->hook_->IsHookActive())
                return (TFunc)this->hook_->GetTrampolineAddress();

            return (TFunc)this->hook_->GetOriginalFunctionAddress();
        }

        inline TFunc GetChainedFunc() const
        {
            if (!this->HasHook())
                return nullptr;

            return (TFunc)this->hook_->GetOriginalFunctionAddress();
        }
    };

    // ------------------------------------------------------------------------------------------

    template<class... TArgs>
    class NitroFunctionVoid : public NitroFunctionBase<void, TArgs...>
    {
    public:
        void operator()(TArgs... args) const
        {
            if (!this->HasHook())
                return;

            this->hook_->InvokeOriginal(std::forward<TArgs>(args)...);
        }

        void InvokeChained(TArgs... args) const
        {
            if (!this->HasHook())
                return;

            this->hook_->Invoke(std::forward<TArgs>(args)...);
        }

        std::shared_ptr<Unsubscriber> operator|=(const std::function<void(TArgs..., NextHandlerInterface<void, TArgs...>*)>& func)
        {
            auto unsubscriber = (*this->chain_) |= func;

            NitroFunctionBase<void, TArgs...>::AttachHookIfNeeded();

            return unsubscriber;
        }

        std::shared_ptr<Unsubscriber> operator+=(const std::function<void(TArgs...)>& func)
        {
            auto unsubscriber = (*this->chain_) += func;

            NitroFunctionBase<void, TArgs...>::AttachHookIfNeeded();

            return unsubscriber;
        }
    };

    template<FunctionPointer TFunc, class... TArgs>
    class NitroFunctionVoidTyped : public NitroFunctionVoid<TArgs...>
    {
    public:
        inline TFunc GetFunc() const
        {
            if (!this->HasHook())
                return nullptr;

            if (this->hook_->IsHookActive())
                return (TFunc)this->hook_->GetTrampolineAddress();

            return (TFunc)this->hook_->GetOriginalFunctionAddress();
        }

        inline TFunc GetChainedFunc() const
        {
            if (!this->HasHook())
                return nullptr;

            return (TFunc)this->hook_->GetOriginalFunctionAddress();
        }
    };
}