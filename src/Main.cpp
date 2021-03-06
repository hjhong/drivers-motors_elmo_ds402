#include <iostream>
#include <canbus.hh>
#include <memory>
#include <motors_elmo_ds402/Controller.hpp>
#include <string>
#include <iomanip>

using namespace std;
using namespace motors_elmo_ds402;

int usage()
{
    cout << "motors_elmo_ds402_ctl CAN_DEVICE CAN_DEVICE_TYPE CAN_ID COMMAND\n";
    cout << "  reset     # resets the drive";
    cout << "  get-state # displays the drive's internal state\n";
    cout << "  set-state NEW_STATE # changes the drive's internal state\n";
    cout << endl;
    return 1;
}

const char* stateToString(StatusWord::State state)
{
    switch(state)
    {
        case StatusWord::NOT_READY_TO_SWITCH_ON: return "NOT_READY_TO_SWITCH_ON";
        case StatusWord::SWITCH_ON_DISABLED: return "SWITCH_ON_DISABLED";
        case StatusWord::READY_TO_SWITCH_ON: return "READY_TO_SWITCH_ON";
        case StatusWord::SWITCH_ON: return "SWITCH_ON";
        case StatusWord::OPERATION_ENABLED: return "OPERATION_ENABLED";
        case StatusWord::QUICK_STOP_ACTIVE: return "QUICK_STOP_ACTIVE";
        case StatusWord::FAULT_REACTION_ACTIVE: return "FAULT_REACTION_ACTIVE";
        case StatusWord::FAULT: return "FAULT";
        default:
            throw std::invalid_argument("unknown state");
    }
}

ControlWord::Transition transitionFromString(std::string const& string)
{
    if (string == "SHUTDOWN") return ControlWord::SHUTDOWN;
    if (string == "SWITCH_ON") return ControlWord::SWITCH_ON;
    if (string == "ENABLE_OPERATION") return ControlWord::ENABLE_OPERATION;
    if (string == "DISABLE_VOLTAGE") return ControlWord::DISABLE_VOLTAGE;
    if (string == "QUICK_STOP") return ControlWord::QUICK_STOP;
    if (string == "DISABLE_OPERATION") return ControlWord::DISABLE_OPERATION;
    if (string == "FAULT_RESET") return ControlWord::FAULT_RESET;
    throw std::invalid_argument("unexpected state transition " + string);
}

static void writeObject(canbus::Driver& device, canbus::Message const& query,
    motors_elmo_ds402::Controller& controller,
    base::Time timeout = base::Time::fromMilliseconds(100))
{

    device.write(query);
    device.setReadTimeout(timeout.toMilliseconds());
    while(true)
    {
        canbus::Message msg = device.read();
        if (controller.process(msg).isAck()) {
            return;
        }
    }
}

static void writeObjects(canbus::Driver& device, vector<canbus::Message> const& query,
    motors_elmo_ds402::Controller& controller,
    base::Time timeout = base::Time::fromMilliseconds(100))
{
    for(auto const& msg : query) {
        writeObject(device, msg, controller, timeout);
    }
}

static void queryObject(canbus::Driver& device, canbus::Message const& query,
    motors_elmo_ds402::Controller& controller,
    uint64_t updateId,
    base::Time timeout = base::Time::fromMilliseconds(100))
{
    device.write(query);
    device.setReadTimeout(timeout.toMilliseconds());
    while(true)
    {
        canbus::Message msg = device.read();
        if (controller.process(msg).isUpdated(updateId)) {
            return;
        }
    }
}

static void queryObjects(canbus::Driver& device, std::vector<canbus::Message> const& query,
    motors_elmo_ds402::Controller& controller,
    uint64_t updateId,
    base::Time timeout = base::Time::fromMilliseconds(100))
{
    for(auto const& msg : query) {
        queryObject(device, msg, controller, updateId, timeout);
    }
}

int main(int argc, char** argv)
{
    if (argc < 5) {
        return usage();
    }

    std::string can_device(argv[1]);
    std::string can_device_type(argv[2]);
    int8_t node_id(stoi(argv[3]));
    std::string cmd(argv[4]);

    unique_ptr<canbus::Driver> device(canbus::openCanDevice(can_device, can_device_type));
    Controller controller(node_id);

    if (cmd == "reset")
    {
        if (argc != 5)
            return usage();

        queryObject(*device,
            controller.queryNodeStateTransition(canopen_master::NODE_RESET),
            controller, UPDATE_HEARTBEAT, base::Time::fromMilliseconds(5000));
        controller.getNodeState();
    }
    else if (cmd == "get-state")
    {
        if (argc != 5)
            return usage();

        queryObject(*device, controller.queryStatusWord(), controller,
            UPDATE_STATUS_WORD);
        StatusWord status = controller.getStatusWord();
        cout << stateToString(status.state) << "\n"
            << "  voltageEnabled      " << status.voltageEnabled << "\n"
            << "  warning             " << status.warning << "\n"
            << "  targetReached       " << status.targetReached << "\n"
            << "  internalLimitActive " << status.internalLimitActive << std::endl;

        queryObjects(*device, controller.queryFactors(),
            controller, UPDATE_FACTORS);
        queryObjects(*device, controller.queryJointState(),
            controller, UPDATE_JOINT_STATE);
        auto jointState = controller.getJointState();
        cout << "Current joint state:\n" <<
            "  position " << jointState.position << "\n" <<
            "  speed    " << jointState.speed << "\n" <<
            "  effort   " << jointState.effort << "\n" <<
            "  current  " << jointState.raw << endl;
    }
    else if (cmd == "get-config")
    {
        queryObjects(*device, controller.queryFactors(),
            controller, UPDATE_FACTORS);
        Factors factors = controller.getFactors();
        cout << "Scale factors:\n"
            << "  positionEncoderResolution " << factors.positionEncoderResolution << "\n"
            << "  velocityEncoderResolution " << factors.velocityEncoderResolution << "\n"
            << "  velocityFactor     " << factors.velocityFactor << "\n"
            << "  accelerationFactor " << factors.accelerationFactor << "\n"
            << "  gearRatio    " << factors.gearRatio << "\n"
            << "  feedConstant " << factors.feedConstant << "\n"
            << "  ratedTorque  " << factors.ratedTorque << "\n"
            << "  ratedCurrent " << factors.ratedCurrent << endl;

        queryObjects(*device, controller.queryJointLimits(),
            controller, UPDATE_JOINT_LIMITS);
        auto jointLimits = controller.getJointLimits();
        cout << "Current joint limits:\n" <<
            "  position     [" << jointLimits.min.position << ", " << jointLimits.max.position << "]\n" <<
            "  speed        [" << jointLimits.min.speed << ", " << jointLimits.max.speed << "]\n" <<
            "  acceleration [" << jointLimits.min.acceleration << ", " << jointLimits.max.acceleration << "]\n" <<
            "  effort       [" << jointLimits.min.effort << ", " << jointLimits.max.effort << "]\n" <<
            "  current      [" << jointLimits.min.raw << ", " << jointLimits.max.raw << "]" << endl;
    }
    else if (cmd == "set-state")
    {
        if (argc != 6)
            return usage();

        auto transition = transitionFromString(argv[5]);
        writeObject(*device, controller.send(ControlWord(transition, true)), controller);
    }
    else if (cmd == "monitor-joint-state")
    {
        queryObjects(*device, controller.queryFactors(),
            controller, UPDATE_FACTORS);
        writeObject(*device,
            controller.send(ControlWord(ControlWord::SHUTDOWN, true)),
            controller);
        writeObjects(*device,
            controller.queryPeriodicJointStateUpdate(1, base::Time::fromMilliseconds(100)),
            controller);
        writeObject(*device,
            controller.send(ControlWord(ControlWord::SWITCH_ON, true)),
            controller);

        canbus::Message sync = controller.querySync();
        device->write(sync);

        cout << setw(10) << "Position" << " "
            << setw(10) << "Speed" << " "
            << setw(10) << "Effort" << " " 
            << setw(10) << "Current" << endl;
        while(true)
        {
            device->write(sync);
            canbus::Message msg = device->read();
            if (controller.process(msg).isUpdated(UPDATE_JOINT_STATE))
            {
                base::JointState jointState = controller.getJointState();
                cout << setw(10) << jointState.position << " "
                    << setw(10) << jointState.speed << " "
                    << setw(10) << jointState.effort << " " 
                    << setw(10) << jointState.raw << endl;
            }
        }
    }
    return 0;
}
