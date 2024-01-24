from tqdm import tqdm
import torch
import torch.nn as nn
from torch.utils.data import Dataset, DataLoader
import pandas as pd
import numpy as np

CUDA = False
if CUDA:
    device = torch.device('cuda')

class PositionDataset(Dataset):
    def __init__(self, file):
        print("Reading dataset")
        self.data = torch.tensor(pd.read_csv(
            file, header=None).astype(np.float32).values)
        print("Done reading")

        # Scale columns in the same way that eval() does so that coefficients have the same order of magnitude
        self.data[:, 1] /= 10000
        self.data[:, 2] /= 10000
        self.data[:, 3] /= 10000
        self.data[:, 4] /= 10000
        self.data[:, 8] /= 10000
        self.data[:, 9] /= 10000
        
        self.data[:, 1+10] /= 10000
        self.data[:, 2+10] /= 10000
        self.data[:, 3+10] /= 10000
        self.data[:, 4+10] /= 10000
        self.data[:, 8+10] /= 10000
        self.data[:, 9+10] /= 10000
    
    def __len__(self):
        return len(self.data)

    def __getitem__(self, idx):
        return self.data[idx]


class LogisticModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.linear_middlegame = nn.Linear(10, 1, bias=False)

    def forward(self, x):
        score = self.linear_middlegame(x[:, :10]-x[:, 10:20])[:, 0]
        return score


TRAIN_SPLIT = 0.8
BS = 16384
LR = 0.001
# games_output should be csv file with no header, 21 values per row
# <10 eval scores for white>,<10 eval scores for black>,<game outcome (0, 0.5, or 1)>
dataset = PositionDataset("./games_tuning_output.txt")
train_size = int(TRAIN_SPLIT*len(dataset))
test_size = len(dataset)-train_size
train_dataset, test_dataset = torch.utils.data.random_split(
    dataset, [train_size, test_size])
train_loader, test_loader = DataLoader(train_dataset, batch_size=BS, shuffle=True, num_workers=8), DataLoader(
    test_dataset, batch_size=BS, shuffle=True, num_workers=8)
model = LogisticModel()
if CUDA:
    model = model.to(device)
criterion = nn.BCEWithLogitsLoss()
optim = torch.optim.Adam(model.parameters(), lr=LR)
# usually 20-30 epochs is sufficient
# train/test loss should be around 4e-5 (or lower)
for epoch in range(30):
    tot_train_loss = 0
    tot_test_loss = 0
    tot_train, tot_test = 0, 0
    for i, data in enumerate(tqdm(train_loader)):
        if CUDA:
            data = data.to(device)
        tot_train += len(data)
        x = data[:, :20]
        y = data[:, -1]
        optim.zero_grad()
        out = model(x).squeeze(-1)
        loss = criterion(out, y)
        # L2 regularization
        loss += torch.sum(torch.square(model.linear_middlegame.weight))*3e-6
        loss.backward()
        optim.step()
        tot_train_loss += loss.item()
        print(f"Epoch {epoch}: train loss {tot_train_loss/tot_train}")
    with torch.no_grad():
        for i, data in enumerate(test_loader):
            if CUDA:
                data = data.to(device)
            tot_test += len(data)
            x = data[:, :20]
            y = data[:, -1]
            out = model(x).squeeze(-1)
            loss = criterion(out, y)
            tot_test_loss += loss.item()
    print(f"Epoch {epoch}: test loss {tot_test_loss/tot_test}")
    torch.set_printoptions(sci_mode=False)
    
    # in eval() PMAT is implicitly 1 so we scale everything to make this true
    scaled1 = (model.linear_middlegame.weight)[
        0]/model.linear_middlegame.weight[0, 7]
    # these should be multiplied by P_EV_VAL when copied into the iopts array
    print(scaled1)
