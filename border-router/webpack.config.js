const path = require('path');

var webpack = require("webpack");

module.exports = {
	entry: {
		site: './web/js/site.js',
		index: ['./web/js/index.js'],
		devices: ['./web/js/devices.js'],
		reports: ['./web/js/reports.js']
	},
	output: {
		filename: '[name].entry.js',
		path: path.resolve(__dirname, '.', 'wwwroot', 'dist')
	},
	devtool: 'source-map',
	mode: 'development',
	module: {
		rules: [
			{ test: /\.css$/, use: ['style-loader', 'css-loader'] },
			{ test: /\.eot(\?v=\d+\.\d+\.\d+)?$/, use: ['file-loader'] },
			{
				test: /\.(woff|woff2)$/, use: [
					{
						loader: 'url-loader',
						options: {
							limit: 5000,
						},
					},
				]
			},
			{
				test: /\.ttf(\?v=\d+\.\d+\.\d+)?$/, use: [
					{
						loader: 'url-loader',
						options: {
							limit: 10000,
							mimetype: 'application/octet-stream',
						},
					},
				]
			},
			{
				test: /\.svg(\?v=\d+\.\d+\.\d+)?$/, use: [
					{
						loader: 'url-loader',
						options: {
							limit: 10000,
							mimetype: 'image/svg+xml',
						},
					},
				]
			}
		]
	},
};
