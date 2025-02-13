import csv
import os

def Analyzer(csv_path, root_dir):
    """
    1) Identifies missing file (present in CSV but not found).
    2) Identifies expected folders not found.
    3) Identifies files that exist but not present in the csv.
    4) Checks if a file in the wrong folder compared to what's listed in the CSV.
    """

    expected_files_dict = {}
    expected_pairs = set()
    expected_folder_names = set()

    with open(csv_path, 'r', newline='', encoding='utf-8') as csvfile:
        reader = csv.DictReader(csvfile)
        for idx, row in enumerate(reader, start=1):
            indicator_class = row.get('indicator_class')
            indicator_name = row.get('indicator_name')

            if indicator_class and not indicator_name:
                parts = [p.strip() for p in indicator_class.split(',')]
                if len(parts) >= 2:
                    folder_raw, name_raw = parts[0], parts[1]
                    folder = folder_raw.lower().replace(' ', '_')
                    filename = name_raw.lower().replace(' ', '_') + ".py"
                else:
                    print(f"[DEBUG] Row {idx} incomplete data, skipping: {row}")
                    continue
            elif indicator_class and indicator_name:
                folder = indicator_class.lower().replace(' ', '_')
                filename = indicator_name.lower().replace(' ', '_') + ".py"
            else:
                print(f"[DEBUG] Skipping row {idx} (missing 'indicator_class'/'indicator_name'): {row}")
                continue

            expected_pairs.add((folder, filename))
            expected_folder_names.add(folder)
            if filename not in expected_files_dict:
                expected_files_dict[filename] = set()
            expected_files_dict[filename].add(folder)

    existing_pairs = set()
    existing_folders = set()

    for dirpath, dirnames, filenames in os.walk(root_dir):
        current_folder = os.path.basename(dirpath).lower().replace(' ', '_')
        existing_folders.add(current_folder)

        for file in filenames:
            if file == '__init__.py' or not file.endswith('.py'):
                continue
            file_lower = file.lower().replace(' ', '_')
            pair = (current_folder, file_lower)
            existing_pairs.add(pair)

    missing_pairs = sorted(expected_pairs - existing_pairs)
    missing_folders = sorted(expected_folder_names - existing_folders)
    extra_pairs = sorted(existing_pairs - expected_pairs)

    wrong_folder = []
    for folder, filename in existing_pairs:
        if filename in expected_files_dict:
           
            valid_folders = expected_files_dict[filename]
            if folder not in valid_folders:
                wrong_folder.append((filename, folder, valid_folders))


    all_ok = True

    if missing_pairs:
        all_ok = False
        print("Missing indicator files (in CSV, not on disk):")
        for folder, filename in missing_pairs:
            print(f" - {folder}/{filename}")

    if missing_folders:
        all_ok = False
        print("\nExpected folders not found on disk:")
        for folder in missing_folders:
            print(f" - {folder}")

    if extra_pairs:
        all_ok = False
        print("\nExtra Python files on disk (not listed in CSV):")
        for folder, filename in extra_pairs:
            print(f" - {folder}/{filename}")

    if wrong_folder:
        all_ok = False
        print("\nFiles found in a folder different from what the CSV expects:")
        for (filename, found_folder, valid_folders) in wrong_folder:
            valid_list = ", ".join(sorted(valid_folders))
            print(f" - {found_folder}/{filename} (expected folder(s): {valid_list})")

    if all_ok:
        print("All files are present. No missing files, extra files, or wrong folders!")


if __name__ == "__main__":
    csv_path = "CSV_PATH"
    root_dir = "ROOT"
    Analyzer(csv_path, root_dir)
