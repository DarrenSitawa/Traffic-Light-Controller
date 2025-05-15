#include <iostream>
#include <queue>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <iomanip>
#include <algorithm>
#include <map>

enum Direction { NORTH = 0, EAST, SOUTH, WEST };
enum LightState { RED, YELLOW, GREEN };
enum PedestrianState { DONT_WALK, WALK };

// Vehicle class
class Vehicle {
private:
    int id;
    bool emergency;
    std::chrono::system_clock::time_point arrivalTime;

public:
    Vehicle(int vehicleId, bool isEmergency = false) : id(vehicleId), emergency(isEmergency) {
        arrivalTime = std::chrono::system_clock::now();
    }

    int getId() const { return id; }
    bool isEmergencyVehicle() const { return emergency; }

    int getWaitingTime() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - arrivalTime).count();
    }
};

// Pedestrian signal
class PedestrianSignal {
private:
    PedestrianState state;
    bool request;

public:
    PedestrianSignal() : state(DONT_WALK), request(false) {}

    void requestCrossing() { request = true; }
    void grantCrossing() { state = WALK; request = false; }
    void endCrossing() { state = DONT_WALK; }

    bool isRequested() const { return request; }
    PedestrianState getState() const { return state; }
};

// Traffic lane
class TrafficLane {
private:
    Direction direction;
    std::queue<Vehicle> vehicles;
    int trafficDensity;

public:
    TrafficLane(Direction dir) : direction(dir), trafficDensity(5) {}

    void addVehicle(const Vehicle& vehicle) { vehicles.push(vehicle); }
    bool hasVehicles() const { return !vehicles.empty(); }
    int getQueueLength() const { return vehicles.size(); }

    Vehicle processVehicle() {
        if (vehicles.empty()) throw std::runtime_error("No vehicles to process");
        Vehicle v = vehicles.front();
        vehicles.pop();
        return v;
    }

    int getTotalWaitTime() const {
        int total = 0;
        std::queue<Vehicle> temp = vehicles;
        while (!temp.empty()) {
            total += temp.front().getWaitingTime();
            temp.pop();
        }
        return total;
    }

    double getAverageWaitTime() const {
        if (vehicles.empty()) return 0;
        return static_cast<double>(getTotalWaitTime()) / vehicles.size();
    }

    Direction getDirection() const { return direction; }
    void setTrafficDensity(int d) { trafficDensity = std::clamp(d, 0, 10); }
    int getTrafficDensity() const { return trafficDensity; }

    bool hasEmergencyVehicle() const {
        std::queue<Vehicle> temp = vehicles;
        while (!temp.empty()) {
            if (temp.front().isEmergencyVehicle()) return true;
            temp.pop();
        }
        return false;
    }
};

// Traffic signal
class TrafficSignal {
private:
    std::vector<LightState> lightStates;
    int currentGreenDirection;
    int baseGreenTime, yellowTime, minGreenTime, maxGreenTime;

public:
    TrafficSignal() : lightStates(4, RED), currentGreenDirection(-1), baseGreenTime(20), yellowTime(3), minGreenTime(10), maxGreenTime(60) {}

    void changeLight(Direction newGreenDir) {
        if (currentGreenDirection >= 0) lightStates[currentGreenDirection] = RED;
        lightStates[newGreenDir] = GREEN;
        currentGreenDirection = newGreenDir;
    }

    void setYellow() {
        if (currentGreenDirection >= 0) lightStates[currentGreenDirection] = YELLOW;
    }

    LightState getLightState(Direction dir) const { return lightStates[dir]; }
    int getCurrentGreenDirection() const { return currentGreenDirection; }
    int getYellowTime() const { return yellowTime; }

    int calculateAdaptiveGreenTime(const TrafficLane& lane) const {
        int time = baseGreenTime + lane.getQueueLength() * 2 + lane.getTrafficDensity() * 2;
        return std::clamp(time, minGreenTime, maxGreenTime);
    }
};

// Intersection Controller
class IntersectionController {
private:
    std::vector<TrafficLane> lanes;
    std::map<Direction, PedestrianSignal> pedestrianSignals;
    TrafficSignal signal;
    std::mt19937 rng;
    int vehicleCounter, totalVehiclesProcessed, totalWaitTime, cycleCounter;

public:
    IntersectionController() : vehicleCounter(0), totalVehiclesProcessed(0), totalWaitTime(0), cycleCounter(0) {
        for (int i = 0; i < 4; ++i) {
            lanes.emplace_back(static_cast<Direction>(i));
            pedestrianSignals[static_cast<Direction>(i)] = PedestrianSignal();
        }
        rng.seed(std::random_device{}());
    }

    void generateTraffic() {
        std::uniform_int_distribution<int> arrivalDist(0, 10);
        std::uniform_int_distribution<int> densityChange(0, 20);
        std::uniform_int_distribution<int> emergencyChance(0, 20);

        for (auto& lane : lanes) {
            if (densityChange(rng) == 0) lane.setTrafficDensity(rng() % 11);
            int arrivalThreshold = 10 - lane.getTrafficDensity();
            if (arrivalDist(rng) >= arrivalThreshold) {
                bool isEmergency = (emergencyChance(rng) == 0);
                lane.addVehicle(Vehicle(++vehicleCounter, isEmergency));
            }
        }

        // Simulate random pedestrian requests
        std::uniform_int_distribution<int> pedChance(0, 15);
        for (auto& [dir, signal] : pedestrianSignals) {
            if (pedChance(rng) == 0) signal.requestCrossing();
        }
    }

    void processCycle() {
        ++cycleCounter;
        std::cout << "\n=== Traffic Cycle #" << cycleCounter << " ===\n";
        generateTraffic();
        displayQueueStatus();

        Direction nextDir = findNextGreenDirection();
        if (signal.getCurrentGreenDirection() >= 0) {
            signal.setYellow();
            std::cout << "Yellow light for " << directionToString(static_cast<Direction>(signal.getCurrentGreenDirection())) << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(signal.getYellowTime()));
        }

        signal.changeLight(nextDir);
        int greenTime = signal.calculateAdaptiveGreenTime(lanes[nextDir]);

        std::cout << "Green light for " << directionToString(nextDir) << " (" << greenTime << "s)\n";

        if (pedestrianSignals[nextDir].isRequested()) {
            pedestrianSignals[nextDir].grantCrossing();
            std::cout << "Pedestrians WALK on " << directionToString(nextDir) << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(3));
            pedestrianSignals[nextDir].endCrossing();
        }

        processVehicles(nextDir, greenTime);
        displayStats();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    Direction findNextGreenDirection() {
        for (const auto& lane : lanes) {
            if (lane.hasEmergencyVehicle()) {
                std::cout << "Emergency vehicle detected on " << directionToString(lane.getDirection()) << "!\n";
                return lane.getDirection();
            }
        }

        double maxScore = -1;
        Direction best = NORTH;
        for (const auto& lane : lanes) {
            if (lane.hasVehicles()) {
                double score = lane.getQueueLength() * lane.getAverageWaitTime() * (1 + lane.getTrafficDensity() / 10.0);
                if (score > maxScore) {
                    maxScore = score;
                    best = lane.getDirection();
                }
            }
        }
        return best;
    }

    void processVehicles(Direction dir, int greenTime) {
        int canPass = greenTime / 2, passed = 0;
        while (passed < canPass && lanes[dir].hasVehicles()) {
            Vehicle v = lanes[dir].processVehicle();
            std::cout << "Vehicle #" << v.getId() << (v.isEmergencyVehicle() ? " (EMERGENCY)" : "")
                << " passed from " << directionToString(dir)
                << " after waiting " << v.getWaitingTime() << "s\n";
            totalVehiclesProcessed++;
            totalWaitTime += v.getWaitingTime();
            passed++;
        }
        std::cout << "Total vehicles passed: " << passed << "\n";
    }

    void displayQueueStatus() const {
        std::cout << "\n--- Queue Status ---\n";
        for (const auto& lane : lanes) {
            Direction d = lane.getDirection();
            std::cout << directionToString(d) << ": " << lane.getQueueLength()
                << " vehicles, Avg Wait: " << std::fixed << std::setprecision(1)
                << lane.getAverageWaitTime() << "s, Density: " << lane.getTrafficDensity()
                << ", Ped Request: " << (pedestrianSignals.at(d).isRequested() ? "Yes" : "No") << "\n";
        }
    }

    void displayStats() const {
        std::cout << "\n--- Statistics ---\n";
        std::cout << "Total Vehicles Processed: " << totalVehiclesProcessed << "\n";
        if (totalVehiclesProcessed)
            std::cout << "Average Wait Time: " << std::fixed << std::setprecision(2)
            << (double)totalWaitTime / totalVehiclesProcessed << "s\n";
    }

    static std::string directionToString(Direction d) {
        switch (d) {
        case NORTH: return "North";
        case EAST: return "East";
        case SOUTH: return "South";
        case WEST: return "West";
        default: return "Unknown";
        }
    }
};

int main() {
    IntersectionController controller;
    int cycles = 20;
    for (int i = 0; i < cycles; ++i) controller.processCycle();
    return 0;
}
