import pandas as pd
import pytz
from dateutil.parser import parse
import matplotlib.pyplot as plt


candlestick_data = pd.read_csv('EURUSD_Candlestick_5_M_ASK_01.01.2023-04.01.2025.csv')
candlestick_data['Gmt time'] = pd.to_datetime(candlestick_data['Gmt time'], format='%d.%m.%Y %H:%M:%S.%f')
candlestick_data['Gmt time'] = candlestick_data['Gmt time'].dt.tz_localize('GMT')


trades_data = pd.read_excel("C:\\Users\\PC\\Downloads\\trades_TimeApprox.xlsx")
trades_data.columns = trades_data.columns.str.strip()

trades_data['Date'] = pd.to_datetime(trades_data['Date'], format='%d/%m/%Y %H:%M:%S')
trades_data['Entry'] = trades_data['Entry'].astype(str).str.replace(',', '.').astype(float)
trades_data['TP'] = trades_data['TP'].astype(str).str.replace(',', '.').astype(float)
trades_data['SL'] = trades_data['SL'].astype(str).str.replace(',', '.').astype(float)
trades_data['BE'] = pd.to_numeric(trades_data['BE'].astype(str).str.replace(',', '.'), errors='coerce')
trades_data['Management'] = pd.to_numeric(trades_data['Management'].astype(str).str.replace(',', '.'), errors='coerce')


eurusd_trades = trades_data[trades_data['Asset'] == 'EURUSD']
eurusd_trades = eurusd_trades[(eurusd_trades['Date'] >= '2023-01-01') & (eurusd_trades['Date'] <= '2025-01-04')]


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
        return None, None, None
    
    exit_candle = trade_data[(trade_data['Low'] <= exit_price) & (trade_data['High'] >= exit_price)]
    if exit_candle.empty:
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
        return None, None, None
    
    return return_pct, mdd * 100, exit_time

def process_trade(trade, candlestick_data):
    trade_date_str = trade['Date'].strftime('%d/%m/%Y %H:%M:%S')
    trade_type = trade['TradeType']
    entry_price = trade['Entry']
    tp = trade['TP']
    sl = trade['SL']
    state = trade['State']
    be = trade['BE']
    management = trade['Management']
    
    if state.lower() == "lose":
        if be == 1 and pd.isna(management):
            exit_price = entry_price * 1.001
        elif be == 1 and not pd.isna(management):
            exit_price = management
        else:
            exit_price = sl
    elif state.lower() == "win":
        exit_price = tp
    else:
        return None, None
    
    gmt_entry_time = paris_to_gmt(trade_date_str)
    start_time = gmt_entry_time - pd.Timedelta(hours=12)
    end_time = gmt_entry_time + pd.Timedelta(hours=12)
    filtered_data = candlestick_data[(candlestick_data['Gmt time'] >= start_time) & (candlestick_data['Gmt time'] <= end_time)]
    
    if filtered_data.empty:
        return None, None
    
    match_found = False
    for index, row in filtered_data.iterrows():
        if row['Low'] <= entry_price <= row['High']:
            match_time = row['Gmt time']
            match_price = entry_price
            match_found = True
            break
    
    if not match_found:
        if len(filtered_data) == 0:
            return None, None
        closest_index = (filtered_data['Gmt time'] - gmt_entry_time).abs().idxmin()
        closest_candle = filtered_data.loc[closest_index]

        match_time = closest_candle['Gmt time']
        match_price = closest_candle[['Open', 'High', 'Low', 'Close']].mean()
    
    return_pct, mdd, exit_time = calculate_return_and_mdd(match_time, match_price, trade_type, exit_price, candlestick_data)
    if return_pct is None or mdd is None:
        return None, None
    
    risk = 0.01
    calmar = return_pct / (-mdd if mdd < 0 else risk)
    
    result = {
        'Date': trade['Date'],
        'Asset': trade['Asset'],
        'TradeType': trade_type,
        'Entry': entry_price,
        'Exit': exit_price,
        'Return (%)': return_pct,
        'MDD (%)': mdd,
        'Calmar': calmar,
        'Exit Time': gmt_to_paris(exit_time)
    }
    
    return result, None

results = []
no_results = []

for index, trade in eurusd_trades.iterrows():
    result, no_result = process_trade(trade, candlestick_data)
    if result:
        results.append(result)
    else:
        no_results.append(trade)

results_df = pd.DataFrame(results)
no_results_df = pd.DataFrame(no_results)

results_df.to_csv('C:\\Users\\PC\\Downloads\\results.csv', index=False)
no_results_df.to_csv('C:\\Users\\PC\\Downloads\\no_results.csv', index=False)

print("Results saved to 'results.csv' and 'no_results.csv'.")
