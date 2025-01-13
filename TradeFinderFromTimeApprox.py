import pandas as pd
import pytz
from dateutil.parser import parse
import matplotlib.pyplot as plt

data = pd.read_csv('EURUSD_Candlestick_5_M_ASK_01.01.2023-04.01.2025.csv')
data['Gmt time'] = pd.to_datetime(data['Gmt time'], format='%d.%m.%Y %H:%M:%S.%f')
data['Gmt time'] = data['Gmt time'].dt.tz_localize('GMT')

def paris_to_gmt(paris_time_str):
    paris_tz = pytz.timezone('Europe/Paris')
    paris_time_str_fixed = paris_time_str.replace('/', '.')
    paris_time = parse(paris_time_str_fixed)
    paris_time = paris_tz.localize(paris_time, is_dst=None)
    gmt_time = paris_time.astimezone(pytz.timezone('GMT'))
    return gmt_time

def gmt_to_paris(gmt_time):
    paris_tz = pytz.timezone('Europe/Paris')
    paris_time = gmt_time.astimezone(paris_tz)
    return paris_time.strftime('%d/%m/%Y %H:%M:%S')

def calculate_return_and_mdd(entry_time, entry_price, trade_type, exit_price, data):
    trade_data = data[data['Gmt time'] >= entry_time]
    if trade_data.empty:
        print("No data found after the entry time.")
        return None, None, None
    
    exit_candle = trade_data[(trade_data['Low'] <= exit_price) & (trade_data['High'] >= exit_price)]
    if exit_candle.empty:
        print("No exit price found after the entry time.")
        return None, None, None
    
    exit_time = exit_candle.iloc[0]['Gmt time']
    exit_price_actual = exit_candle.iloc[0]['Close']
    
    prices = trade_data[trade_data['Gmt time'] <= exit_time]['Close']
    
    if trade_type.lower() == "long":
        cumulative_max = prices.cummax()
        drawdown = (prices - cumulative_max) / cumulative_max
        mdd = drawdown.min()
        return_pct = ((exit_price_actual - entry_price) / entry_price) * 100
    elif trade_type.lower() == "short":
        cumulative_min = prices.cummin()
        drawup = (cumulative_min - prices) / cumulative_min
        mdd = drawup.min()
        return_pct = ((entry_price - exit_price_actual) / entry_price) * 100
    else:
        print("Invalid trade type. Use 'long' or 'short'.")
        return None, None, None
    
    return return_pct, mdd * 100, exit_time

def find_and_plot(trade_date_str, trade_type, entry_price, exit_price, risk, data):
    gmt_entry_time = paris_to_gmt(trade_date_str)
    
    start_time = gmt_entry_time - pd.Timedelta(hours=12)
    end_time = gmt_entry_time + pd.Timedelta(hours=12)
    filtered_data = data[(data['Gmt time'] >= start_time) & (data['Gmt time'] <= end_time)]
    
    if filtered_data.empty:
        print("No data found within the ±12-hour window.")
        return
    
    match_found = False
    for index, row in filtered_data.iterrows():
        if row['Low'] <= entry_price <= row['High']:
            match_time = row['Gmt time']
            match_price = entry_price
            print(f"Price within HL range found at: {gmt_to_paris(match_time)} (Paris time)")
            match_found = True
            break
    
    if not match_found:
        closest_candle = filtered_data.iloc[(filtered_data['Gmt time'] - gmt_entry_time).abs().idxmin()]
        match_time = closest_candle['Gmt time']
        match_price = closest_candle[['Open', 'High', 'Low', 'Close']].mean()
        percentage_diff = ((entry_price - match_price) / match_price) * 100
        print(f"No match found within HL range. Closest candle time: {gmt_to_paris(match_time)} (Paris time), Price: {match_price:.5f}, Percentage difference: {percentage_diff:.2f}%")
    
    return_pct, mdd, exit_time = calculate_return_and_mdd(match_time, match_price, trade_type, exit_price, data)
    if return_pct is not None and mdd is not None:
        print(f"Return: {return_pct:.2f}%")
        print(f"Maximum Drawdown (MDD): {mdd:.2f}%")
        print(f"Calmar: {return_pct/-mdd if mdd < 0 else return_pct/risk:.2f}")
        print(f"Exit time: {gmt_to_paris(exit_time)} (Paris time)")
    
    typo = "v"
    if(trade_type.lower() == "short"):
        typo = "^"
    plt.figure(figsize=(20, 6))
    plt.scatter(match_time, entry_price, color="Green", label ="True")
    plt.scatter(gmt_entry_time, entry_price, marker='x', color="Orange", label ="Approx")
    plt.axhline(exit_price, color="Orange", label ="Exit Price")

    plt.plot(filtered_data['Gmt time'], filtered_data['High'], label='High Price', color='DarkGray')
    plt.plot(filtered_data['Gmt time'], filtered_data['Low'], label='Low Price', color='DarkGray')
    plt.plot(filtered_data['Gmt time'], filtered_data['Close'], label='Close Price', color='white')

    
    plt.title(f"Price Movement on {gmt_entry_time.date()} (±12 Hours)")
    plt.xlabel('Time (GMT)')
    plt.ylabel('Price')
    plt.legend()
    plt.grid(False)
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.show()

trade_date = "25/04/2024 14:30:00"
trade_type = "short"
entry_price = 1.07386
exit_price = 1.07159
risk = 0.01 # instead of return calmar=inf due to no MDD/MDU lets do return/risk (which will always lead to high ratio, perfectly since there is no DD)

find_and_plot(trade_date, trade_type, entry_price, exit_price, risk, data)
