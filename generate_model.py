import torch
import torch.nn as nn
import os

# Define a lightweight dummy model for telemetry anomaly detection
class DummyAnomalyDetector(nn.Module):
    def __init__(self):
        super().__init__()
        # Simulating an input of 10 sensor readings
        self.network = nn.Sequential(
            nn.Linear(10, 16),
            nn.ReLU(),
            nn.Linear(16, 1),
            nn.Sigmoid() # Outputs an anomaly probability score between 0 and 1
        )

    def forward(self, x):
        return self.network(x)

# Initialize model and set to evaluation mode
model = DummyAnomalyDetector()
model.eval()

# Create dummy telemetry input (batch_size=1, sensors=10)
example_input = torch.rand(1, 10)

# Trace the model to TorchScript
traced_script_module = torch.jit.trace(model, example_input)

# Create the models directory if it doesn't exist
os.makedirs("astramind_ws/src/astramind_core/models", exist_ok=True)

# Save the model directly into the ROS2 package
save_path = "astramind_ws/src/astramind_core/models/astramind_model.pt"
traced_script_module.save(save_path)

print(f"✅ Successfully generated TorchScript model at: {save_path}")