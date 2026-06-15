const path = require("path");
const HtmlWebpackPlugin = require("html-webpack-plugin");
const CopyWebpackPlugin = require("copy-webpack-plugin");
const webpack = require("webpack");

module.exports = (_env, argv) => {
  const isProd = argv.mode === "production";
  return {
    entry: "./src/index.tsx",
    output: {
      path: path.resolve(__dirname, "dist"),
      filename: isProd ? "[name].[contenthash].js" : "[name].js",
      publicPath: "/",
      clean: true,
    },
    resolve: {
      extensions: [".ts", ".tsx", ".js", ".json"],
    },
    devtool: isProd ? "source-map" : "inline-source-map",
    module: {
      rules: [
        {
          test: /\.tsx?$/,
          exclude: /node_modules/,
          use: {
            loader: "ts-loader",
            options: { transpileOnly: true },
          },
        },
        {
          test: /\.css$/,
          use: ["style-loader", "css-loader"],
        },
        {
          test: /\.(png|jpg|jpeg|gif|svg)$/,
          type: "asset/resource",
        },
      ],
    },
    plugins: [
      new HtmlWebpackPlugin({
        template: "./public/index.html",
      }),
      // Ship the ported landing assets (images + audio) into the build output.
      new CopyWebpackPlugin({
        patterns: [{ from: "public/landing", to: "landing" }],
      }),
      new webpack.DefinePlugin({
        "process.env.API_BASE_URL": JSON.stringify(process.env.API_BASE_URL || ""),
        "process.env.STRIPE_PUBLISHABLE_KEY": JSON.stringify(
          process.env.STRIPE_PUBLISHABLE_KEY || ""
        ),
      }),
    ],
    devServer: {
      static: path.resolve(__dirname, "public"),
      historyApiFallback: true,
      port: 3000,
      hot: true,
      proxy: [
        {
          context: ["/api", "/uploads"],
          target: process.env.API_BASE_URL || "http://localhost:4000",
          changeOrigin: true,
        },
      ],
    },
  };
};
