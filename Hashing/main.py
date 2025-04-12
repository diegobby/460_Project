import hashlib

# Define the string to be hashed
data = "Password123443322HRFWWTAH"

# Perform SHA-256 hashing
hashed_data = hashlib.sha256(data.encode()).hexdigest()

# Print the result
print("SHA-256 Hash:", hashed_data)