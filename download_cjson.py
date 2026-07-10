import urllib.request
import os

try:
    urllib.request.urlretrieve("https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h", "src/cJSON.h")
    urllib.request.urlretrieve("https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c", "src/cJSON.c")
    print("cJSON downloaded successfully.")
except Exception as e:
    print(f"Error: {e}")
