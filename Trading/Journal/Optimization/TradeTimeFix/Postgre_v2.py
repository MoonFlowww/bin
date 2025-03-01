import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

file_path = r"C:\Users\PC\Downloads\output\EURUSD_ticks.csv\EURUSD_ticks.csv"

data = pd.read_csv(file_path)
data['Local time'] = pd.to_datetime(data['Timestamp'], errors='coerce').dt.tz_localize(None)
data['Ask'] = pd.to_numeric(data['Ask'], errors='coerce')
data['Bid'] = pd.to_numeric(data['Bid'], errors='coerce')
data['Mid'] = (data['Ask'] + data['Bid']) / 2


"""
# Connect to Postgres and fetch data.
engine = create_engine('postgresql://postgres:Monarch@localhost:5432/PallasDB')
query = 'SELECT timestamp, ask, bid, ask_volume, bid_volume FROM "EURUSD_tickdata"'
data = pd.read_sql(query, engine, parse_dates=['Local time'])
"""



def calculate_return_and_mdd(entry_time, entry_price, trade_type, exit_price, exit_mode, data, risk, min_wait=pd.Timedelta(minutes=1)):
    df = data[data['Local time'] >= entry_time].copy()
    if df.empty:
        print("No data after entry.")
        return None, None, None

    # Standard TP/SL exit conditions.
    if exit_mode.upper() in ["TP", "SL"]:
        if trade_type.lower() == "long":
            cond = df['Bid'] >= exit_price if exit_mode.upper() == "TP" else df['Bid'] <= exit_price
            exit_rows = df[cond]
            if exit_rows.empty:
                print("No exit tick found for", exit_mode)
                return None, None, None
            exit_time = exit_rows.iloc[0]['Local time']
            exit_price_actual = exit_rows.iloc[0]['Bid']
            prices = df[df['Local time'] <= exit_time]['Bid']
            mdd = ((prices - prices.cummax()) / prices.cummax()).min()
            ret = ((exit_price_actual - entry_price) / (entry_price - sl)) * risk
        elif trade_type.lower() == "short":
            cond = df['Ask'] <= exit_price if exit_mode.upper() == "TP" else df['Ask'] >= exit_price
            exit_rows = df[cond]
            if exit_rows.empty:
                print("No exit tick found for", exit_mode)
                return None, None, None
            exit_time = exit_rows.iloc[0]['Local time']
            exit_price_actual = exit_rows.iloc[0]['Ask']
            prices = df[df['Local time'] <= exit_time]['Ask']
            mdd = ((prices.cummin() - prices) / prices.cummin()).min()
            ret = ((entry_price - exit_price_actual) / (sl - entry_price)) * risk

    # Partial exit uses SMA crossover and a dynamic waiting period.
    elif exit_mode.upper() == "PARTIAL":
        ma_window = 10 #sma window for reversal identification
        if trade_type.lower() == "long":
            df['SMA'] = df['Bid'].rolling(window=ma_window, min_periods=1).mean()
            cond_T1 = (df['SMA'].shift(1) > exit_price) & (df['SMA'] <= exit_price)
            candidates = df[cond_T1]
            if candidates.empty:
                print("No reversal found for partial exit (long).")
                return None, None, None
            T1 = candidates.iloc[0]['Local time']
            tp_rows = df[df['Bid'] >= exit_price]
            if tp_rows.empty:
                print("No TP candidate found for partial exit (long).")
                return None, None, None
            T_tp = tp_rows.iloc[0]['Local time']
            waiting_period = pd.Timedelta(seconds=(T_tp - entry_time).total_seconds() * 0.2)
            if waiting_period < min_wait:
                waiting_period = min_wait
            threshold_time = T1 + waiting_period
            print(f"(Long) Reversal condition: T1 = {T1}, T_tp = {T_tp}, entry = {entry_time}, waiting_period = {waiting_period}, threshold_time = {threshold_time}")
            later_df = df[df['Local time'] >= threshold_time]
            if later_df.empty:
                exit_row = candidates.iloc[0]
            else:
                idx = (later_df['Bid'] - exit_price).abs().idxmin()
                exit_row = later_df.loc[idx]
            exit_time = exit_row['Local time']
            exit_price_actual = exit_row['Bid']
            prices = df[df['Local time'] <= exit_time]['Bid']
            mdd = ((prices - prices.cummax()) / prices.cummax()).min()
            ret = ((exit_price_actual - entry_price) / (entry_price - sl)) * risk

        elif trade_type.lower() == "short":
            df['SMA'] = df['Ask'].rolling(window=ma_window, min_periods=1).mean()
            cond_T1 = (df['SMA'].shift(1) < exit_price) & (df['SMA'] >= exit_price)
            candidates = df[cond_T1]
            if candidates.empty:
                print("No reversal found for partial exit (short).")
                return None, None, None
            T1 = candidates.iloc[0]['Local time']
            tp_rows = df[df['Ask'] <= exit_price]
            if tp_rows.empty:
                print("No TP candidate found for partial exit (short).")
                return None, None, None
            T_tp = tp_rows.iloc[0]['Local time']
            waiting_period = pd.Timedelta(seconds=(T_tp - entry_time).total_seconds() * 0.2)
            if waiting_period < min_wait:
                waiting_period = min_wait
            threshold_time = T1 + waiting_period
            print(f"(Short) Reversal condition: T1 = {T1}, T_tp = {T_tp}, entry = {entry_time}, waiting_period = {waiting_period}, threshold_time = {threshold_time}")
            later_df = df[df['Local time'] >= threshold_time]
            if later_df.empty:
                exit_row = candidates.iloc[0]
            else:
                idx = (later_df['Ask'] - exit_price).abs().idxmin()
                exit_row = later_df.loc[idx]
            exit_time = exit_row['Local time']
            exit_price_actual = exit_row['Ask']
            prices = df[df['Local time'] <= exit_time]['Ask']
            mdd = ((prices.cummin() - prices) / prices.cummin()).min()
            ret = ((entry_price - exit_price_actual) / (sl - entry_price)) * risk
    else:
        print("Invalid exit mode.")
        return None, None, None

    return ret, mdd * 100, exit_time

def find_and_plot(trade_date_str, trade_type, entry_price, exit_price, sl, risk, exit_mode, data):
    entry_time = pd.to_datetime(trade_date_str, errors='coerce')
    initial_window = data[(data['Local time'] >= entry_time - pd.Timedelta(hours=12)) &
                          (data['Local time'] <= entry_time + pd.Timedelta(hours=12))]
    if initial_window.empty:
        print("No data in initial ±12h window.")
        return

    price_col = 'Ask' if trade_type.lower() == 'long' else 'Bid'
    closest_idx = (initial_window[price_col] - entry_price).abs().idxmin()
    closest = initial_window.loc[closest_idx]
    tol = 0.0001
    if abs(closest[price_col] - entry_price) < tol:
        match_time, match_price = closest['Local time'], entry_price
        print(f"Exact tick at: {match_time}")
    else:
        match_time, match_price = closest['Local time'], closest[price_col]
        diff = ((entry_price - match_price) / match_price) * 100
        print(f"No exact match. Closest tick at {match_time}, price {match_price:.5f}, diff {diff:.2f}%")

    ret, mdd, exit_time = calculate_return_and_mdd(match_time, match_price, trade_type, exit_price, exit_mode, data, risk)
    if ret is not None:
        # Ulcer Index calculation.
        trade_df = data[(data['Local time'] >= match_time) & (data['Local time'] <= exit_time)]
        if trade_df.empty:
            print("Not enough trade data for Ulcer Index.")
            return
        if trade_type.lower() == "long":
            dd = (trade_df['Bid'] - trade_df['Bid'].cummax()) / trade_df['Bid'].cummax() * 100
        else:
            dd = (trade_df['Ask'].cummin() - trade_df['Ask']) / trade_df['Ask'].cummin() * 100
        UI = np.sqrt((dd**2).mean())
        UPI = (ret - risk) / UI if UI != 0 else np.inf
        CALMAR = (ret - risk) / abs(mdd) if abs(mdd) >= 0 else np.inf
        print(f"Return: {ret:.2f}%")
        print(f"Risk: {risk:.2f}%")

        print(f"MDD: {-mdd:.2f}%")
        print(f"Calmar: {UPI:.2f}")
        
        print(f"UI: {UI:.2f}%") #Ulcer Index
        print(f"UPI: {UPI:.2f}")
        print(f"Exit Time: {exit_time}")

    # plot from entry+12hrs to exit+12hrs
    extended_window = data[(data['Local time'] >= entry_time - pd.Timedelta(hours=12)) &
                           (data['Local time'] <= exit_time + pd.Timedelta(hours=12))]
    print(f"Plotting from {entry_time - pd.Timedelta(hours=12)} to {exit_time + pd.Timedelta(hours=12)}")
    
    plt.figure(figsize=(20, 10))
    
    plt.plot(extended_window['Local time'], extended_window['Ask'], color='linen', label='Ask Price', linewidth=0.75, alpha=0.75)
    plt.plot(extended_window['Local time'], extended_window['Bid'], color='bisque', label='Bid Price', linewidth=0.75, alpha=0.75)

    EntrySignal = "v"
    ExitSignal = "^"
    if(trade_type == "Long" or trade_type == "long"):
        EntrySignal = "^"
        ExitSignal = "v"
    
    plt.scatter(match_time, entry_price, marker=EntrySignal, color="lime", label="Entry Tick", s=150)
    plt.scatter(entry_time, entry_price, marker='x', color="aqua", label="Approx Entry", s=50)
    if ret is not None:
        plt.scatter(exit_time, exit_price, marker=ExitSignal, color="red", label="Exit Tick", s=150)
    plt.title(f"Price Movement from {entry_time - pd.Timedelta(hours=12)} to {exit_time + pd.Timedelta(hours=12)}")
    plt.xlabel('Time (Local)')
    plt.ylabel('Price')
    plt.legend()
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.show()

# Parameters:
trade_date = "31/10/2023 09:45:00"
trade_type = "short"
entry_price = 1.06643
exit_price = 1.05706
sl = 1.06882
risk = 3.3
exit_mode = "Partial"              # "TP", "SL", or "Partial"

find_and_plot(trade_date, trade_type, entry_price, exit_price, sl, risk, exit_mode, data)
