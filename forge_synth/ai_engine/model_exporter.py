import torch
import torch.nn as nn
import numpy as np
import onnx
import onnxruntime as ort

class ForgeNeuralSynth(nn.Module):
    \"\"\"
    A VAE-inspired Neural Wavetable Generator.
    Maps a 2D latent vector to a 2048-sample wavetable.
    \"\"\"
    def __init__(self):
        super(ForgeNeuralSynth, self).__init__()
        self.decoder = nn.Sequential(
            nn.Linear(2, 64),
            nn.ReLU(),
            nn.Linear(64, 256),
            nn.ReLU(),
            nn.Linear(256, 2048),
            nn.Tanh() # Normalize output between -1 and 1
        )

    def forward(self, z):
        return self.decoder(z)

def export_to_onnx(model_path=\"forge_synth/models/forge_synth_v1.onnx\"):
    model = ForgeNeuralSynth()
    model.eval()
    
    # Create dummy input for ONNX trace (Batch size 1, Latent dim 2)
    dummy_input = torch.randn(1, 2)
    
    torch.onnx.export(
        model, 
        dummy_input, 
        model_path, 
        export_params=True, 
        opset_version=11, 
        do_constant_folding=True, 
        input_names=['latent_input'], 
        output_names=['wavetable_output']
    )
    print(f\"Model successfully exported to {model_path}\")

if __name__ == \"__main__\":
    import os
    os.makedirs(\"forge_synth/models\", exist_ok=True)
    export_to_onnx()
