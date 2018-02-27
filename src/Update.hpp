#ifndef MOTORS_ELMO_DS402_UPDATE_HPP
#define MOTORS_ELMO_DS402_UPDATE_HPP

#include <cstdint>

namespace motors_elmo_ds402
{
    class Update
    {
        uint64_t updated_objects;

    public:
        Update(uint16_t updated_objects)
            : updated_objects(updated_objects) {}

        template<typename T>
        bool isUpdated() const
        {
            return (updated_objects & T::UPDATE_ID);
        }
    };
}

#endif
