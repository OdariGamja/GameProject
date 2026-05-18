# Game Title
The Subway

## Overview
Linux環境上で開発したネットワーク型マルチプレイ対戦ゲーム。

### ルール
制限時間内に、より多くのスコアを獲得したプレイヤーが勝利するパーティゲーム。

- 最大4人でプレイ可能
- 他のプレイヤーを攻撃したり、ステージ外へ押し出して倒すことが可能
- ATM付近でスペースキーを押すとコインが出現
- コインを自販機に入れることでスコアを獲得できる

## Tech Stack
- C
- SDL2
- SDL_image
- SDL_ttf
- Linux

## My Contributions
- 物理判定およびレンダリングにおけるズームイン・ズームアウトのスケール処理を実装
- サーバー中心型のブロードキャスト通信システムを実装
- アニメーションシステムを実装
- マルチスレッドによるアニメーション処理を実装

## Challenges

### 2.5Dスケール処理
物理判定用座標、カメラ処理、その他すべてのオブジェクト描画においてスケール処理が必要だった。

## Screenshots
<img width="1050" height="630" alt="illust" src="https://github.com/user-attachments/assets/a365fe91-bf3d-4863-b7ca-1962e4df1e25" />



## How to Build
Linux環境で実行する必要がある

1.サーバーの起動
  ip addr でサーバ側のipアドレスを検索
  上から2番目の有線ネットワークの192.xxx.x.xxを利用
  ./server で実行

2.クライアントの起動
  ./client サーバーを立てたPCのipアドレスで実行
  例)./client 192.xxx.x.xx
