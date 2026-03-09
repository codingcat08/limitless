# limitless
limit() called
     ↓
rate limiter allows request
     ↓
LimitGuard created
     ↓
API call runs
     ↓
guard goes out of scope
     ↓
destructor runs automatically

This pattern is called RAII
RAII = Resource Acquisition Is Initialization
Meaning:
resource acquired when object is created
resource released when object is destroyed
Example in the standard library:
std::lock_guard<std::mutex> lock(m);
When lock goes out of scope → mutex unlocks automatically.
Your LimitGuard works the same way.

