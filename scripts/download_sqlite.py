import urllib.request
import zipfile
import os
import shutil

URL = "https://www.sqlite.org/2024/sqlite-amalgamation-3450300.zip"
ZIP_FILE = "sqlite.zip"
SRC_DIR = "../src"

print(f"Downloading SQLite amalgamation from {URL}...")
urllib.request.urlretrieve(URL, ZIP_FILE)

print("Extracting...")
with zipfile.ZipFile(ZIP_FILE, 'r') as zip_ref:
    zip_ref.extractall(".")

folder_name = "sqlite-amalgamation-3450300"

print("Moving sqlite3.c and sqlite3.h to src directory...")
shutil.move(os.path.join(folder_name, "sqlite3.c"), os.path.join(SRC_DIR, "sqlite3.c"))
shutil.move(os.path.join(folder_name, "sqlite3.h"), os.path.join(SRC_DIR, "sqlite3.h"))

print("Cleaning up...")
os.remove(ZIP_FILE)
shutil.rmtree(folder_name)

print("Done! SQLite is ready for compilation.")
