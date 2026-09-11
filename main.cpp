#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdlib>

using namespace std;
using namespace std::chrono;

// --- DEFINITIONS ---
#define MAX_TRAINS 100
#define CAPACITY 500
#define BOOK_MIN 5
#define BOOK_MAX 10
#define MAX_THREADS 20
// GLOBAL CONSTRAINT: Max number of threads allowed to enter the booking system logic
#define MAX_CONCURRENT_ACCESS 5
#define MAX_TIME 1 // mins

// --- GLOBAL SHARED RESOURCES ---
// 1. Mutexes for Data Integrity (Fine-grained locking)
std::mutex train_mutex[MAX_TRAINS];
int available_seats[MAX_TRAINS];

// 2. Resources for Global Load Management (Condition Variable Logic)
std::mutex access_mutex; // Protects the access_count
std::condition_variable access_cond; // Signals when an access slot is freed
int active_access_count = 0; // Current number of threads inside the critical region

// 3. Thread Management Variables
std::thread threads[MAX_THREADS];
int num_threads = 0;

// 4. Output Control
std::mutex print_mutex;

// --- HELPER FUNCTIONS (Unchanged) ---
int get_random_train() {
    return std::rand() % MAX_TRAINS;
}

int get_random_bookings() {
    return std::rand() % (BOOK_MAX - BOOK_MIN + 1) + BOOK_MIN;
}

void print_query(int thread_num, int type, int train_num, const string& action) {
    lock_guard<std::mutex> lock(print_mutex);
    cout << "Thread " << thread_num << ": " << action;
    if (type == 1) cout << " Inquiry";
    else if (type == 2) cout << " Booking";
    else if (type == 3) cout << " Cancellation";
    cout << " on Train " << train_num << endl;
}

// --- WORKER THREAD (FIXED) ---
void worker_thread(int thread_num) {
    auto start = std::chrono::steady_clock::now();

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 500));
        int train_num = get_random_train();
        int type = std::rand() % 3 + 1;

        // Check time limit before starting a new request
        auto now = std::chrono::steady_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start);
        if (elapsed_seconds.count() >= MAX_TIME * 60) {
            break;
        }

        // --- PHASE 1: GLOBAL LOAD CONTROL (Using Condition Variable) ---
        { // Start a new scope for unique_lock
            print_query(thread_num, type, train_num, "WAITING for system access.");

            // Acquire access_mutex
            std::unique_lock<std::mutex> load_lock(access_mutex);

            // Wait until an access slot is free (releases lock while waiting)
            access_cond.wait(load_lock, [&]{
                return active_access_count < MAX_CONCURRENT_ACCESS;
            });

            // Lock re-acquired, condition met.
            active_access_count++; // Claim the slot

            // load_lock is released here when scope ends, ensuring active_access_count is protected.
        } // End of scope: load_lock releases access_mutex automatically.

        print_query(thread_num, type, train_num, "GAINED system access.");

        // --- PHASE 2: LOCAL DATA INTEGRITY (Using Train Mutex) ---

        // Acquire lock for the specific train to ensure data integrity
        std::lock_guard<std::mutex> train_lock(train_mutex[train_num]);

        // Execute Query (Critical Section for data)
        lock_guard<std::mutex> print_lock(print_mutex);

        switch (type) {
            case 1: { // Inquiry (Read)
                cout << "Thread " << thread_num << ": Train " << train_num
                     << " has " << available_seats[train_num] << " seats available." << endl;
                break;
            }
            case 2: { // Booking (Write)
                int num_to_book = get_random_bookings();
                if (available_seats[train_num] >= num_to_book) {
                    available_seats[train_num] -= num_to_book;
                    cout << "Thread " << thread_num << ": SUCCESSFULLY BOOKED " << num_to_book
                         << " seats in Train " << train_num << ". Remaining: "
                         << available_seats[train_num] << endl;
                } else {
                    cout << "Thread " << thread_num << ": FAILED to book in Train " << train_num << "." << endl;
                }
                break;
            }
            case 3: { // Cancellation (Write)
                int booked_seats = CAPACITY - available_seats[train_num];
                if (booked_seats > 0) {
                    int num_to_cancel = std::rand() % booked_seats + 1;
                    available_seats[train_num] += num_to_cancel;
                    cout << "Thread " << thread_num << ": SUCCESSFULLY CANCELLED " << num_to_cancel
                         << " seats in Train " << train_num << ". Remaining: "
                         << available_seats[train_num] << endl;
                } else {
                    cout << "Thread " << thread_num << ": Train " << train_num
                         << " has no bookings to cancel." << endl;
                }
                break;
            }
        }
        // train_lock and print_lock are released here.

        // --- PHASE 3: RELEASE GLOBAL ACCESS (Signaling) ---

        { // Start a new scope for load_lock
            // Re-acquire the global load lock to safely decrement the counter
            std::lock_guard<std::mutex> release_lock(access_mutex);
            active_access_count--; // Release the slot
        } // End of scope: release_lock releases access_mutex automatically.

        // Signal one waiting thread that a slot in the global access pool is free
        access_cond.notify_one();

        // Time check moved to the start of the loop for cleaner structure.
    }
}

// --- MAIN FUNCTION (Unchanged) ---
int main() {
    std::srand(std::time(nullptr));
    for (int i = 0; i < MAX_TRAINS; i++) {
        available_seats[i] = CAPACITY;
    }

    // Creating and running the worker threads
    for (int i = 0; i < MAX_THREADS; i++) {
        threads[i] = std::thread(worker_thread, i);
        num_threads++;
    }

    // Wait for all threads to finish
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }

    cout << "\n--- Final Reservation Chart ---\n";
    cout << "    Train number    Available Seats\n";
    for(int i = 0; i < MAX_TRAINS; i++){
        cout << "        " << i << "                " << available_seats[i] << endl;
    }
    cout << "Thanks for using our services!!!\n";

    return 0;
}
