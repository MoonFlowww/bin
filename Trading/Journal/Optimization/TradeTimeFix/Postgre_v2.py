import pandas as pd
import pytz
from dateutil.parser import parse
import matplotlib.pyplot as plt
from sqlalchemy import create_engine

# Connect to Postgres and fetch data.
engine = create_engine('postgresql://postgres:Monarch@localhost:5432/PallasDB')
query = 'SELECT timestamp, ask, bid, ask_volume, bid_volume FROM "EURUSD_tickdata"'
data = pd.read_sql(query, engine, parse_dates=['timestamp'])
data.rename(columns={'timestamp': 'Gmt time'}, inplace=True)

# DB timestamps are stored as "2023-01-01 01:00:12.304+01" (Paris time).
# Convert them to GMT.
data['Gmt time'] = data['Gmt time'].dt.tz_convert('GMT')
# Create mid-price column.
data['price'] = (data['ask'] + data['bid']) / 2

# trade_date is provided in Paris time. Convert it to GMT.
def paris_to_gmt(paris_time_str):
    paris_tz = pytz.timezone('Europe/Paris')
    paris_time = parse(paris_time_str)
    return paris_tz.localize(paris_time, is_dst=None).astimezone(pytz.timezone('GMT'))

def gmt_to_paris(gmt_time):
    paris_tz = pytz.timezone('Europe/Paris')
    return gmt_time.astimezone(paris_tz).strftime('%d/%m/%Y %H:%M:%S')

def calculate_return_and_mdd(entry_time, entry_price, trade_type, exit_price, data):
    trade_data = data[data['Gmt time'] >= entry_time]
    if trade_data.empty:
        print("No data after entry.")
        return None, None, None

    if trade_type.lower() == "long":
        exit_tick = trade_data[trade_data['price'] >= exit_price]
        if exit_tick.empty:
            print("No exit tick for long.")
            return None, None, None
        exit_row = exit_tick.iloc[0]
        exit_time = exit_row['Gmt time']
        exit_price_actual = exit_row['price']
        prices = trade_data['price']
        mdd = ((prices - prices.cummax()) / prices.cummax()).min()
        ret_pct = ((exit_price_actual - entry_price) / entry_price) * 100
    elif trade_type.lower() == "short":
        exit_tick = trade_data[trade_data['price'] <= exit_price]
        if exit_tick.empty:
            print("No exit tick for short.")
            return None, None, None
        exit_row = exit_tick.iloc[0]
        exit_time = exit_row['Gmt time']
        exit_price_actual = exit_row['price']
        prices = trade_data['price']
        mdd = ((prices.cummin() - prices) / prices.cummin()).min()
        ret_pct = ((entry_price - exit_price_actual) / entry_price) * 100
    else:
        print("Invalid trade type.")
        return None, None, None

    return ret_pct, mdd * 100, exit_time

def find_and_plot(trade_date_str, trade_type, entry_price, exit_price, risk, data):
    # Convert trade_date from Paris time to GMT.
    gmt_entry_time = paris_to_gmt(trade_date_str)
    window = data[(data['Gmt time'] >= gmt_entry_time - pd.Timedelta(hours=12)) &
                  (data['Gmt time'] <= gmt_entry_time + pd.Timedelta(hours=12))]
    if window.empty:
        print("No data in ±12h window.")
        return

    window['diff'] = (window['price'] - entry_price).abs()
    closest = window.loc[window['diff'].idxmin()]
    match_time, match_price = closest['Gmt time'], closest['price']
    diff_pct = ((entry_price - match_price) / match_price) * 100
    print(f"Closest tick: {gmt_to_paris(match_time)}; Price: {match_price:.5f}; Diff: {diff_pct:.2f}%")

    ret_pct, mdd, exit_time = calculate_return_and_mdd(match_time, match_price, trade_type, exit_price, data)
    if ret_pct is not None:
        calmar = ret_pct / (-mdd if mdd < 0 else risk)
        print(f"Return: {ret_pct:.2f}% | MDD: {mdd:.2f}% | Calmar: {calmar:.2f}")
        print(f"Exit time: {gmt_to_paris(exit_time)}")

    plt.figure(figsize=(20, 6))
    plt.scatter(match_time, entry_price, color="green", label="Entry True", s=50)
    plt.scatter(gmt_entry_time, entry_price, marker='x', color="orange", label="Entry Approx", s=50)
    plt.scatter(exit_time, exit_price, color="red", label="Exit True", s=50)
    plt.plot(window['Gmt time'], window['price'], color='white', label='Price')
    plt.title(f"Price Movement on {gmt_entry_time.date()} (±12h)")
    plt.xlabel('Time (GMT)')
    plt.ylabel('Price')
    plt.legend()
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.show()


# Parameters and execution.
trade_date = "31/10/2023  09:45:00"
trade_type = "short"
entry_price = 1.06643
exit_price = 1.05706
risk = 0.033

find_and_plot(trade_date, trade_type, entry_price, exit_price, risk, data)
