import ftplib
import os
from urllib.parse import urlparse
from pathlib import Path
import time
import glob
import psutil
import databento as db
import zstandard as zstd
from tqdm import tqdm
import threading
import tempfile
import gc

from concurrent.futures import as_completed
from concurrent.futures import ProcessPoolExecutor

def get_remote_file_size(ftp, filename):
    try:
        ftp.voidcmd('TYPE I')
        size = ftp.size(filename)
        return size
    except ftplib.error_perm:
        try:
            files = []
            ftp.retrlines('LIST', lambda x: files.append(x))
            for file_info in files:
                parts = file_info.split()
                if len(parts) >= 9:
                    file_name = ' '.join(parts[8:])
                    if file_name == filename:
                        return int(parts[4])
        except:
            pass
    return None

def check_download_status(local_filepath, remote_size):
    if not os.path.exists(local_filepath):
        return "download", 0
    
    local_size = os.path.getsize(local_filepath)
    
    if remote_size is None:
        print(f"  Warning: Could not determine remote file size for comparison")
        return "skip", local_size
    
    if local_size == remote_size:
        return "complete", local_size
    elif local_size < remote_size:
        return "resume", local_size
    else:
        print(f"  Warning: Local file is larger than remote ({local_size} > {remote_size})")
        return "redownload", 0

def download_from_databento_ftp(ftp_url, local_dir="./input", username=None, password=None, resume=True):
    
    parsed = urlparse(ftp_url)
    host = parsed.hostname
    path = parsed.path
    
    Path(local_dir).mkdir(parents=True, exist_ok=True)
    
    ftp = ftplib.FTP()
    
    try:
        print(f"Connecting to {host}...")
        ftp.connect(host, 21)
        
        if username and password:
            ftp.login(username, password)
            print(f"Logged in as {username}")
        else:
            ftp.login()
            print("Logged in anonymously")
        
        if path and path != '/':
            print(f"Navigating to {path}")
            ftp.cwd(path)
        
        print("\nListing files in directory:")
        files = []
        ftp.retrlines('LIST', lambda x: files.append(x))
        
        file_names = []
        for file_info in files:
            parts = file_info.split()
            if len(parts) >= 9:
                filename = ' '.join(parts[8:])
                file_names.append(filename)
                print(f"  - {filename}")
        
        print(f"\nChecking existing files in {local_dir}...")
        download_plan = {}
        
        for filename in file_names:
            file_listing = files[file_names.index(filename)]
            if file_listing.startswith('d'):
                print(f"Skipping directory: {filename}")
                continue
            
            local_filepath = os.path.join(local_dir, filename)
            
            remote_size = get_remote_file_size(ftp, filename)
            
            status, local_size = check_download_status(local_filepath, remote_size)
            download_plan[filename] = {
                'status': status,
                'local_size': local_size,
                'remote_size': remote_size,
                'local_filepath': local_filepath
            }
            
            if status == "complete":
                print(f"✓ {filename} - Already complete ({local_size / (1024*1024):.2f} MB)")
            elif status == "resume":
                print(f"↻ {filename} - Will resume from {local_size / (1024*1024):.2f} MB of {remote_size / (1024*1024):.2f} MB")
            elif status == "download" or status == "redownload":
                size_str = f"({remote_size / (1024*1024):.2f} MB)" if remote_size else "(unknown size)"
                print(f"↓ {filename} - Will download {size_str}")
            elif status == "skip":
                print(f"? {filename} - Exists locally, skipping (cannot verify size)")
        
        print(f"\nStarting downloads...")
        for filename, plan in download_plan.items():
            if plan['status'] == 'complete' or plan['status'] == 'skip':
                continue
            
            local_filepath = plan['local_filepath']
            local_size = plan['local_size']
            remote_size = plan['remote_size']
            
            try:
                if plan['status'] == 'resume' and resume:
                    with open(local_filepath, 'ab') as local_file:
                        print(f"Resuming {filename} from {local_size / (1024*1024):.2f} MB...", end='', flush=True)
                        start_time = time.time()
                        
                        ftp.voidcmd(f'REST {local_size}')
                        
                        downloaded = [local_size]
                        def handle_binary(data):
                            local_file.write(data)
                            downloaded[0] += len(data)
                            if remote_size:
                                progress = (downloaded[0] / remote_size) * 100
                                print(f"\r  Resuming {filename}... {downloaded[0] / (1024*1024):.2f} MB ({progress:.1f}%)", end='', flush=True)
                            else:
                                print(f"\r  Resuming {filename}... {downloaded[0] / (1024*1024):.2f} MB", end='', flush=True)
                        
                        ftp.retrbinary(f'RETR {filename}', handle_binary, blocksize=1024*1024)

                        
                        elapsed_time = time.time() - start_time
                        print(f"\r  ✓ Resumed {filename} ({downloaded[0] / (1024*1024):.2f} MB total) in {elapsed_time:.2f} seconds")
                
                else:
                    with open(local_filepath, 'wb', buffering=16*1024*1024) as local_file:
                        print(f"Downloading {filename}...", end='', flush=True)
                        start_time = time.time()
                        
                        file_size = [0]
                        def handle_binary(data):
                            local_file.write(data)
                            file_size[0] += len(data)
                            if remote_size:
                                progress = (file_size[0] / remote_size) * 100
                                print(f"\r  Downloading {filename}... {file_size[0] / (1024*1024):.2f} MB ({progress:.1f}%)", end='', flush=True)
                            else:
                                print(f"\r  Downloading {filename}... {file_size[0] / (1024*1024):.2f} MB", end='', flush=True)
                        
                        ftp.retrbinary(f'RETR {filename}', handle_binary, blocksize=1024*1024)

                        
                        elapsed_time = time.time() - start_time
                        print(f"\r  ✓ Downloaded {filename} ({file_size[0] / (1024*1024):.2f} MB) in {elapsed_time:.2f} seconds")
            
            except Exception as e:
                print(f"\r  ✗ Error downloading {filename}: {e}")
                continue
        
        print("\nDownload process complete!")
        
        completed = sum(1 for plan in download_plan.values() if plan['status'] in ['complete', 'skip'])
        total = len(download_plan)
        print(f"Files processed: {completed}/{total} complete")
        
    except ftplib.error_perm as e:
        print(f"FTP permission error: {e}")
        print("You might need valid credentials to access this FTP server.")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        try:
            ftp.quit()
        except:
            ftp.close()
        print("FTP connection closed.")

def download_single_file_with_resume(ftp_url, local_filepath=None, username=None, password=None, resume=True):
    
    parsed = urlparse(ftp_url)
    host = parsed.hostname
    path = parsed.path
    
    directory = os.path.dirname(path)
    filename = os.path.basename(path)
    
    if not local_filepath:
        local_filepath = filename
    
    local_dir = os.path.dirname(local_filepath)
    if local_dir:
        Path(local_dir).mkdir(parents=True, exist_ok=True)
    
    ftp = ftplib.FTP()
    
    try:
        print(f"Connecting to {host}...")
        ftp.connect(host, 21)
        
        if username and password:
            ftp.login(username, password)
        else:
            ftp.login()
        
        if directory and directory != '/':
            ftp.cwd(directory)
        
        remote_size = get_remote_file_size(ftp, filename)
        
        status, local_size = check_download_status(local_filepath, remote_size)
        
        if status == "complete":
            print(f"✓ {filename} is already complete ({local_size / (1024*1024):.2f} MB)")
            return
        
        if status == "resume" and resume:
            with open(local_filepath, 'ab') as local_file:
                print(f"Resuming {filename} from {local_size / (1024*1024):.2f} MB...")
                
                ftp.voidcmd(f'REST {local_size}')
                
                file_size = [local_size]
                def handle_binary(data):
                    local_file.write(data)
                    file_size[0] += len(data)
                    if remote_size:
                        progress = (file_size[0] / remote_size) * 100
                        print(f"\r  Progress: {file_size[0] / (1024*1024):.2f} MB ({progress:.1f}%)", end='', flush=True)
                    else:
                        print(f"\r  Progress: {file_size[0] / (1024*1024):.2f} MB", end='', flush=True)
                
                ftp.retrbinary(f'RETR {filename}', handle_binary, blocksize=1024*1024)
                print(f"\n  ✓ Resumed successfully! Total: {file_size[0] / (1024*1024):.2f} MB")
        else:
            with open(local_filepath, 'wb') as local_file:
                print(f"Downloading {filename}...")
                file_size = [0]
                
                def handle_binary(data):
                    local_file.write(data)
                    file_size[0] += len(data)
                    if remote_size:
                        progress = (file_size[0] / remote_size) * 100
                        print(f"\r  Progress: {file_size[0] / (1024*1024):.2f} MB ({progress:.1f}%)", end='', flush=True)
                    else:
                        print(f"\r  Progress: {file_size[0] / (1024*1024):.2f} MB", end='', flush=True)
                
                ftp.retrbinary(f'RETR {filename}', handle_binary, blocksize=1024*1024)
                print(f"\n  ✓ Downloaded successfully! {file_size[0] / (1024*1024):.2f} MB")
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        try:
            ftp.quit()
        except:
            ftp.close()




def get_ram_usage():
    return psutil.virtual_memory().percent

def update_ram_display(pbar, stop_event):
    while not stop_event.is_set():
        pbar.set_postfix({'RAM': f'{get_ram_usage():.1f}%'})
        pbar.refresh() # force redraw pbar
        time.sleep(0.5)

def convert_dbn_to_parquet_zst(input_path, output_path):
    temp_dir = tempfile.gettempdir()
    temp_parquet = os.path.join(temp_dir, f'temp_{os.getpid()}_{time.time()}.parquet')
    try:
        store = db.DBNStore.from_file(input_path)
        store.to_parquet(temp_parquet)
        store = None
        gc.collect()
        cctx = zstd.ZstdCompressor(level=3)
        with open(temp_parquet, 'rb', buffering=128*1024*1024) as f_in, open(output_path, 'wb', buffering=128*1024*1024) as f_out:
            cctx.copy_stream(f_in, f_out)
    finally:
        if os.path.exists(temp_parquet):
            try:
                os.remove(temp_parquet)
            except:
                pass


def worker(file_path, output_path):
    convert_dbn_to_parquet_zst(file_path, output_path)
    return os.path.basename(file_path)

def process_files(input_folder, output_folder, max_workers=None):
    os.makedirs(output_folder, exist_ok=True)

    dbn_files = glob.glob(os.path.join(input_folder, '*.dbn.zst'))
    if not dbn_files:
        print("No .dbn.zst files found in the input folder")
        return

    processed_log = os.path.join(output_folder, "processed.txt")
    if os.path.exists(processed_log):
        with open(processed_log) as f:
            done = set(line.strip() for line in f)
    else:
        done = set()

    tasks = []
    with tqdm(total=len(dbn_files), desc="Processing files", unit="file") as pbar:
        stop_event = threading.Event()
        ram_thread = threading.Thread(target=update_ram_display, args=(pbar, stop_event))
        ram_thread.daemon = True
        ram_thread.start()

        with ProcessPoolExecutor(max_workers=max_workers) as pool:
            futures = {
                pool.submit(worker, file_path, os.path.join(output_folder, f"{os.path.basename(file_path).replace('.dbn.zst', '')}.parquet.zst")): file_path
                for file_path in dbn_files
                if os.path.basename(file_path).replace('.dbn.zst', '') not in done
            }

            for fut in as_completed(futures):
                file_path = futures[fut]
                try:
                    result = fut.result()
                    with open(processed_log, "a") as f:
                        f.write(result.replace('.dbn.zst','') + "\n")
                except Exception as e:
                    print(f"\nError: {file_path} → {e}")
                pbar.update(1)

        stop_event.set()
        ram_thread.join()
    
    print(f"\nProcessing complete!")
    total_input_size = sum(os.path.getsize(f) for f in dbn_files) / (1024**3)
    total_output_size = sum(os.path.getsize(os.path.join(output_folder, f)) 
                           for f in os.listdir(output_folder) 
                           if f.endswith('.parquet.zst')) / (1024**3)
    print(f"Total input size: {total_input_size:.2f} GB")
    print(f"Total output size: {total_output_size:.2f} GB")





if __name__ == "__main__":
    ftp_url = "_FTPLINK_"
    input_folder = r"_FTP_DOWNLOAD_PATH_" # downloads dir
    output_folder = r"_PARQUET_OUTPUT_PATH_" # parquet output
    
    print("Downloading all files from directory with resume support...")
    download_from_databento_ftp(ftp_url, local_dir=input_folder, username="_EMAIL_", password="_PASSWORD_", resume=True) #resume: fallback if files already got computed


    
    process_files(input_folder, output_folder, round((os.cpu_count()/2)+2)) #.dbn.zst -> .parquet.zst

    print("done\a")

    
