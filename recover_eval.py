import json

def recover():
    with open(r"C:\Users\a2b\.gemini\antigravity-ide\brain\e5186192-3dbd-45cc-9045-56fea424c552\.system_generated\logs\transcript_full.jsonl", "r", encoding="utf-8") as f:
        for line in f:
            data = json.loads(line)
            if "tool_calls" in data:
                for call in data["tool_calls"]:
                    if call["function"]["name"] == "default_api:multi_replace_file_content":
                        args = json.loads(call["function"]["arguments"])
                        if args.get("TargetFile", "").endswith("eval.c"):
                            for chunk in args.get("ReplacementChunks", []):
                                print(f"REPLACEMENT CHUNK: {chunk['StartLine']} to {chunk['EndLine']}")
                                print(chunk["ReplacementContent"])
                                print("="*40)

if __name__ == "__main__":
    recover()
