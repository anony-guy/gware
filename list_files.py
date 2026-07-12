import json

def run():
    files = set()
    with open(r"C:\Users\a2b\.gemini\antigravity-ide\brain\e5186192-3dbd-45cc-9045-56fea424c552\.system_generated\logs\transcript_full.jsonl", "r", encoding="utf-8") as f:
        for line in f:
            data = json.loads(line)
            if "tool_calls" in data:
                for call in data["tool_calls"]:
                    if call["function"]["name"] in ["default_api:write_to_file", "default_api:multi_replace_file_content", "default_api:replace_file_content"]:
                        args = json.loads(call["function"]["arguments"])
                        files.add(args.get("TargetFile", ""))
    for file in files:
        print(file)

if __name__ == "__main__":
    run()
