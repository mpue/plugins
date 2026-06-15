FROM node:20-alpine AS builder
WORKDIR /app

ARG API_BASE_URL=
ARG STRIPE_PUBLISHABLE_KEY=
ENV API_BASE_URL=$API_BASE_URL
ENV STRIPE_PUBLISHABLE_KEY=$STRIPE_PUBLISHABLE_KEY

COPY package.json ./
RUN npm install --no-audit --no-fund

COPY tsconfig.json webpack.config.js ./
COPY public ./public
COPY src ./src

RUN npm run build

FROM nginx:alpine AS runner
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=builder /app/dist /usr/share/nginx/html
EXPOSE 80
CMD ["nginx", "-g", "daemon off;"]
