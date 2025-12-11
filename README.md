# ShowRandomImg

This repository contains an Arduino project and some image scripts for showing random images on a device.

Contents:
- `ShowRandomImage/` — Arduino sketches and headers
- `images/` — image tools and input/output folders

Getting started (create a public GitHub repo and push):

1. Initialize and commit locally (already done):

   ```powershell
   git init
   git add .
   git commit -m "Initial commit"
   ```

2. Create a GitHub repository (two options):

   - Web: Create a new repo on github.com, then follow the instructions to add a remote and push.

   - CLI (if you have `gh` installed):

     ```powershell
     gh repo create <owner>/<repo-name> --public --source=. --remote=origin --push
     ```

3. Or add remote manually and push:

   ```powershell
   git remote add origin https://github.com/<your-username>/<repo-name>.git
   git branch -M main
   git push -u origin main
   ```

Replace `<your-username>` and `<repo-name>` with your GitHub details.
