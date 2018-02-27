#ifndef MOTORS_ELMO_DS402_CONTROLLER_HPP
#define MOTORS_ELMO_DS402_CONTROLLER_HPP

#include <canopen_master/StateMachine.hpp>
#include <motors_elmo_ds402/Objects.hpp>
#include <motors_elmo_ds402/Update.hpp>

namespace motors_elmo_ds402 {
    struct HasPendingQuery : public std::runtime_error {};

    /** Representation of a controller through the CANOpen protocol
     * 
     * This is designed to be independent of _how_ the CAN bus
     * itself is being accessed. It represents only the protocol
     */

    class Controller
    {
        typedef canopen_master::StateMachine StateMachine;

    public:
        Controller(uint8_t nodeId);

        /**
         * Message to query the current status word
         */
        canbus::Message queryStatusWord() const;

        /**
         * Return the last received status word
         */
        StatusWord getStatusWord() const;

        /** Process a can message and returns what got updated
         */
        Update process(canbus::Message const& msg);

    private:
        StateMachine mCanOpen;

        template<typename T> T get() const;
    };
}

#endif