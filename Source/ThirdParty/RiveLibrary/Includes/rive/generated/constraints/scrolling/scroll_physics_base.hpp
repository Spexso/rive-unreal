#ifndef _RIVE_SCROLL_PHYSICS_BASE_HPP_
#define _RIVE_SCROLL_PHYSICS_BASE_HPP_
#include "rive/component.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
namespace rive
{
class ScrollPhysicsBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 523;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScrollPhysicsBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t constraintIdPropertyKey = 731;

protected:
    uint32_t m_ConstraintId = -1;

public:
    inline uint32_t constraintId() const { return m_ConstraintId; }
    void constraintId(uint32_t value)
    {
        if (m_ConstraintId == value)
        {
            return;
        }
        m_ConstraintId = value;
        constraintIdChanged();
    }

    void copy(const ScrollPhysicsBase& object)
    {
        m_ConstraintId = object.m_ConstraintId;
        Component::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case constraintIdPropertyKey:
                m_ConstraintId = CoreUintType::deserialize(reader);
                return true;
        }
        return Component::deserialize(propertyKey, reader);
    }

protected:
    virtual void constraintIdChanged() {}
};
} // namespace rive

#endif